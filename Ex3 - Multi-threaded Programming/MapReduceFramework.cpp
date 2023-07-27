#include "MapReduceFramework.h"
#include "Barrier.h"
#include <iostream>
#include <atomic>
#include <string>
#include <algorithm>
#include <semaphore.h>

#define SYSTEM_ERR "system error: "
#define LOCK_ERR "pthread_mutex_lock failed."
#define UNLOCK_ERR "pthread_mutex_unlock failed."
#define STAGE_OFFSET 62
#define TOTAL_PAIRS_OFFSET 31

/*
 * A struct that represents a thread.
 */
struct Thread {
    pthread_t pthread;
    IntermediateVec intermediateVec;
};

/*
 * A struct which includes all the parameters which are relevant to the job
 * (e.g. the threads, state, mutexes...).
 */
struct JobContext {
    const MapReduceClient *client;
    const InputVec *inputVec;
    OutputVec *outputVec;
    int threadsCount;

    Thread *threads;
    std::vector<IntermediateVec> shuffleOut;

    std::atomic<uint64_t> atomicVar{0};
    std::atomic<uint32_t> atomicShuffle{0};
    Barrier *barrier;
    sem_t semaphore;
    pthread_mutex_t mapMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t reduceMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t emit3Mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t waitMutex = PTHREAD_MUTEX_INITIALIZER;
    bool waitDone = false;
};

/*
 * The function gets a string, prints it to cout and then exists the program.
 */
void printError(const std::string &msg) {
    std::cout << SYSTEM_ERR << msg << std::endl;
    exit(EXIT_FAILURE);
}

/**
* The function saves the intermediary element (K2, V2) in the thread's intermediate vector.
* @param key Intermediary key.
* @param value Intermediary value.
* @param context The job context.
*/
void emit2(K2 *key, V2 *value, void *context) {
    JobContext *jc = (JobContext *) context;
    IntermediateVec *cur;
    for (int i = 0; i < jc->threadsCount; ++i) {
        if (jc->threads[i].pthread == pthread_self()) {
            cur = &(jc->threads[i].intermediateVec);
        }
    }
    cur->push_back(IntermediatePair(key, value));
}

/**
 * The function saves the output element (K3, V3) in the output vector.
 * @param key Output key.
 * @param value Output value.
 * @param context The job context.
 */
void emit3(K3 *key, V3 *value, void *context) {
    JobContext *jc = (JobContext *) context;
    if (pthread_mutex_lock(&jc->emit3Mutex) != 0) {
        printError(LOCK_ERR);
    }
    jc->outputVec->push_back(OutputPair(key, value));
    if (pthread_mutex_unlock(&jc->emit3Mutex) != 0) {
        printError(UNLOCK_ERR);
    }
}

/*
 * Retrieves the num of total pairs from val which represents the atomic variable.
 */
uint64_t getTotalPairs(uint64_t val) {
    // use mask to set to 1 first 31 bits
    uint64_t mask = ((uint64_t) 1 << TOTAL_PAIRS_OFFSET) - 1;
    return (val >> TOTAL_PAIRS_OFFSET) & mask;
}

/*
 * Retrieves the num of pairs processed from val which represents the atomic variable.
 */
uint64_t getPairsProcessed(uint64_t val) {
    // use mask to set to 1 first 31 bits
    uint64_t mask = ((uint64_t) 1 << TOTAL_PAIRS_OFFSET) - 1;
    return val & mask;
}

/*
 * Retrieves the stage from val which represents the atomic variable.
 */
stage_t getStage(uint64_t val) {
    return (stage_t) (val >> STAGE_OFFSET);
}

/*
 * Sets atomicVar's bits as follows:
 * 62-63 for stage, 31-61 for total pairs and 0-30 for processed pairs.
 */
void setAtomicVar(std::atomic<uint64_t> &atomicVar, uint64_t stage, uint64_t totalPairs) {
    atomicVar = (stage << STAGE_OFFSET) | (totalPairs << TOTAL_PAIRS_OFFSET);
}


/*
 * Each thread reads different pairs of (k1, v1) from the input vector
 * and calls the client's map function on each of them.
 */
void mapPhase(JobContext *jc) {
    if (pthread_mutex_lock(&jc->mapMutex) != 0) {
        printError(LOCK_ERR);
    }
    if (getStage(jc->atomicVar) == UNDEFINED_STAGE) {
        setAtomicVar(jc->atomicVar, MAP_STAGE, jc->inputVec->size());
    }
    if (pthread_mutex_unlock(&jc->mapMutex) != 0) {
        printError(UNLOCK_ERR);
    }
    uint64_t oldValue = jc->atomicVar.fetch_add(1);
    oldValue = getPairsProcessed(oldValue);
    if (oldValue >= jc->inputVec->size()) {
        --jc->atomicVar;
        return;
    }
    while (oldValue < jc->inputVec->size()) {
        InputPair pair = (*(jc->inputVec))[oldValue];
        jc->client->map(pair.first, pair.second, jc);
        oldValue = jc->atomicVar.fetch_add(1);
        oldValue = getPairsProcessed(oldValue);
        if (oldValue >= jc->inputVec->size()) {
            --jc->atomicVar;
            return;
        }
    }
}

/*
 * Immediately after the Map phase each thread will sort its intermediate
 * vector according to the keys within.
 */
void sortPhase(JobContext *jc) {
    IntermediateVec *cur;
    for (int i = 0; i < jc->threadsCount; ++i) {
        if (jc->threads[i].pthread == pthread_self()) {
            cur = &(jc->threads[i].intermediateVec);
        }
    }
    sort(cur->begin(), cur->end(),
         [](IntermediatePair &a, IntermediatePair &b) { return *(a.first) < *(b.first); });
}

/*
 * Returns total pairs for the shuffle phase.
 */
uint64_t getTotalShufflePairs(JobContext *jc) {
    uint64_t total = 0;
    for (int i = 0; i < jc->threadsCount; ++i) {
        total += jc->threads[i].intermediateVec.size();
    }
    return total;
}

/*
 * Returns the maximum key out of all intermediate vectors.
 */
K2 *getMaxKey(JobContext *jc) {
    K2 *maxKey = nullptr;
    K2 *lastKey = nullptr;
    IntermediateVec *curVec;
    for (int i = 0; i < jc->threadsCount; ++i) {
        curVec = &(jc->threads[i].intermediateVec);
        if (!curVec->empty()) {
            lastKey = curVec->back().first;
        } else {
            lastKey = nullptr;
        }
        if (maxKey == nullptr || (lastKey != nullptr && *maxKey < *lastKey)) {
            maxKey = lastKey;
        }
    }
    return maxKey;
}

/*
 * Creates new sequences of (k2, v2) where in each sequence all keys are identical and all
 * elements with a given key are in a single sequence.
 */
void shufflePhase(JobContext *jc) {
    K2 *maxKey = getMaxKey(jc);
    IntermediateVec *curVec;
    while (maxKey) {
        IntermediateVec newVec;
        for (int i = 0; i < jc->threadsCount; ++i) {
            curVec = &(jc->threads[i].intermediateVec);
            while (!curVec->empty()) {
                if ((*(curVec->back().first) < *maxKey) || (*maxKey < *(curVec->back().first))) {
                    break;
                }
                newVec.push_back(curVec->back());
                curVec->pop_back();
                ++jc->atomicVar;
            }
        }
        if (!newVec.empty()) {
            jc->shuffleOut.push_back(newVec);
            ++jc->atomicShuffle;
        }
        maxKey = getMaxKey(jc);
    }
}

/*
 * At this point the shuffled vectors have been created, and each thread will run the client's
 * reduce function on different vector.
 */
void reducePhase(JobContext *jc) {
    size_t vecSize;
    while (jc->atomicShuffle-- > 0) {
        if (pthread_mutex_lock(&jc->reduceMutex) != 0) {
            printError(LOCK_ERR);
        }
        if (jc->shuffleOut.empty()) {
            if (pthread_mutex_unlock(&jc->reduceMutex) != 0) {
                printError(UNLOCK_ERR);
            }
            return;
        }
        IntermediateVec vec = jc->shuffleOut.back();
        vecSize = vec.size();
        jc->shuffleOut.pop_back();
        if (pthread_mutex_unlock(&jc->reduceMutex) != 0) {
            printError(UNLOCK_ERR);
        }
        jc->client->reduce(&vec, jc);
        jc->atomicVar += vecSize;
    }
}

/*
 * Each thread will run the map, sort and reduce phases,
 * and only one thread will run the shuffle phase.
 * @param arg The job context.
 * @return nullptr.
 */
void *jobsStartPoint(void *arg) {
    JobContext *jc = (JobContext *) arg;
    mapPhase(jc);
    sortPhase(jc);
    jc->barrier->barrier();

    if (jc->threads[0].pthread == pthread_self()) {
        uint64_t totalShufflePairs = getTotalShufflePairs(jc);
        setAtomicVar(jc->atomicVar, SHUFFLE_STAGE, totalShufflePairs);
        shufflePhase(jc);
        setAtomicVar(jc->atomicVar, REDUCE_STAGE, totalShufflePairs);
        for (int i = 0; i < jc->threadsCount - 1; ++i) {
            if (sem_post(&jc->semaphore) != 0) {
                printError("sem_post failed.");
            }
        }
    } else if (sem_wait(&jc->semaphore) != 0) {
        printError("sem_wait failed.");
    }

    reducePhase(jc);
    return nullptr;
}


/**
 * This function starts running the MapReduce algorithm (with several threads)
 * and returns a JobHandle.
 * @param client The implementation of MapReduceClient or in other words the task
 *               that the framework should run.
 * @param inputVec a vector of type std::vector<std::pair<K1*, V1*>>, the input elements.
 * @param outputVec a vector of type std::vector<std::pair<K3*, V3*>>, to which the output elements
 *                  will be added before returning. You can assume that outputVec is empty.
 * @param multiThreadLevel the number of worker threads to be used for running the algorithm.
 * @return The function returns JobHandle that will be used for monitoring the job.
 */
JobHandle startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec,
                            OutputVec &outputVec, int multiThreadLevel) {
    JobContext *jc = new JobContext;
    jc->client = &client;
    jc->inputVec = &inputVec;
    jc->outputVec = &outputVec;
    jc->threadsCount = multiThreadLevel;
    jc->barrier = new Barrier(multiThreadLevel);
    if (sem_init(&jc->semaphore, 0, 0) != 0) {
        printError("sem_init failed.");
    }

    jc->threads = new Thread[multiThreadLevel];
    for (int i = 0; i < multiThreadLevel; ++i) {
        if (pthread_create(&(jc->threads[i].pthread), nullptr,
                           jobsStartPoint, jc)) {
            printError("pthread_create failed.");
        }
    }
    return jc;
}

/**
 * The function gets JobHandle returned by startMapReduceFramework and waits until it is finished.
 * @param job The job context.
 */
void waitForJob(JobHandle job) {
    JobContext *jc = (JobContext *) job;
    if (pthread_mutex_lock(&jc->waitMutex) != 0) {
        printError(LOCK_ERR);
    }
    if (!jc->waitDone) {
        for (int i = 0; i < jc->threadsCount; ++i) {
            if (pthread_join(jc->threads[i].pthread, nullptr) != 0) {
                printError("pthread_join failed.");
            }
        }
        jc->waitDone = true;
    }
    if (pthread_mutex_unlock(&jc->waitMutex) != 0) {
        printError(UNLOCK_ERR);
    }
}

/**
 * This function gets a JobHandle and updates the state of the job into the given JobState struct.
 * @param job The job context.
 * @param state The state that need to be updated.
 */
void getJobState(JobHandle job, JobState *state) {
    JobContext *jc = (JobContext *) job;
    uint64_t val = jc->atomicVar.load();
    state->stage = getStage(val);
    state->percentage = 0;
    uint64_t totalPairs = getTotalPairs(val);
    if (totalPairs != 0) {
        float p = ((float) 100 * getPairsProcessed(val)) / totalPairs;
        state->percentage = (p > 100) ? 100 : p;
    }
}

/**
 * Releases all resources of a job, only after the job has finished.
 * @param job The job context.
 */
void closeJobHandle(JobHandle job) {
    waitForJob(job);
    JobContext *jc = (JobContext *) job;

    for (int i = 0; i < jc->threadsCount; i++) {
        jc->threads[i].intermediateVec.clear();
    }

    for (IntermediateVec &vec: jc->shuffleOut) {
        vec.clear();
    }
    jc->shuffleOut.clear();

    delete jc->barrier;
    if (sem_destroy(&jc->semaphore) != 0) {
        printError("sem_destroy failed.");
    }
    if (pthread_mutex_destroy(&jc->mapMutex) != 0) {
        printError("pthread_mutex_destroy failed.");
    }
    if (pthread_mutex_destroy(&jc->reduceMutex) != 0) {
        printError("pthread_mutex_destroy failed.");
    }
    if (pthread_mutex_destroy(&jc->emit3Mutex) != 0) {
        printError("pthread_mutex_destroy failed.");
    }
    if (pthread_mutex_destroy(&jc->waitMutex) != 0) {
        printError("pthread_mutex_destroy failed.");
    }

    delete[] jc->threads;
    delete jc;
}
