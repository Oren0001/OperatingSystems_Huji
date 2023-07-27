#include "uthreads.h"
#include "Thread.h"
#include <iostream>
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <list>
#include <deque>
#include <queue>
#include <vector>
#include <algorithm>

using std::cerr;
using std::endl;
using std::string;

#define ERROR -1
#define MICRO_TO_SECS 1000000
#define SYS_FAIL_MSG "system error: "
#define LIB_FAIL_MSG "thread library error: "
#define EMPTY_SET_ERR "sigemptyset error."
#define ADD_SET_ERR "sigaddset error."
#define PROC_MASK_ERR "sigprocmask error."
#define ACTION_ERR "sigaction error."

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}
#endif


enum Error {
    SYSTEM, LIBRARY
};


struct ThreadsData {
    Thread *threads[MAX_THREAD_NUM];
    std::priority_queue<int, std::vector<int>, std::greater<int>> unused_ids;
    std::deque<Thread *> ready_threads;
    struct sigaction sa;
    struct itimerval timer;
    Thread *running_thread;
    sigjmp_buf env[MAX_THREAD_NUM];
    int total_quantums;
};

int Thread::count = 0;
static ThreadsData data;

/*
 * Prints the given message and exists the program for System error, or returns for Library error.
 * @param msg - Message to print in cerr.
 * @param err - The type of the error, System or Library.
 */
static void print_error(const string &msg, Error err) {
    switch (err) {
        case SYSTEM:
            std::cerr << SYS_FAIL_MSG << msg << std::endl;
            exit(EXIT_FAILURE);
        case LIBRARY:
            cerr << LIB_FAIL_MSG << msg << endl;
    }
}

/*
 * Activates an operation (e.g. SIG_BLOCK) on the set of signals - data.sa.sa_mask,
 * by using proc_mask.
 */
static void proc_mask(int how) {
    if (sigprocmask(how, &data.sa.sa_mask, nullptr) == ERROR) {
        print_error(PROC_MASK_ERR, SYSTEM);
    }
}

/*
 * Returns the smallest non-negative integer that is not already taken by an existing thread.
 */
static int produce_id() {
    if (data.unused_ids.empty()) {
        return Thread::count;
    } else {
        int id = data.unused_ids.top();
        data.unused_ids.pop();
        return id;
    }
}

/*
 * Saves stack context, program counter, signal mask and CPU state in env for later use.
 */
static void setup_thread(Thread *thread, thread_entry_point entry_point) {
    proc_mask(SIG_BLOCK);
    sigsetjmp(data.env[thread->get_id()], 1);
    if (thread->get_id() == MAIN_ID) {
        (data.env[MAIN_ID]->__jmpbuf)[JB_SP] = (long) __builtin_frame_address(0);
        (data.env[MAIN_ID]->__jmpbuf)[JB_PC] = (long) __builtin_return_address(0);
    } else {
        address_t sp = (address_t) thread->get_sp() + STACK_SIZE - sizeof(address_t);
        address_t pc = (address_t) entry_point;
        (data.env[thread->get_id()]->__jmpbuf)[JB_SP] = (long) translate_address(sp);
        (data.env[thread->get_id()]->__jmpbuf)[JB_PC] = (long) translate_address(pc);
    }
    if (sigemptyset(&data.env[thread->get_id()]->__saved_mask) == ERROR) {
        print_error(EMPTY_SET_ERR, SYSTEM);
    }
    proc_mask(SIG_UNBLOCK);
}

/*
 * Switch the running thread with the next thread in the ready queue.
 */
static void preempt() {
    proc_mask(SIG_BLOCK);
    if (!data.ready_threads.empty()) {
        Thread *thread = data.running_thread;

        data.running_thread = data.ready_threads.front();
        data.running_thread->set_state(RUNNING);
        data.ready_threads.pop_front();

        if (!thread->get_is_sleeping() && thread->get_state() != BLOCKED) {
            thread->set_state(READY);
            data.ready_threads.push_back(thread);
        }
    }
    data.running_thread->increase_quantums();
    proc_mask(SIG_UNBLOCK);
}

/*
 * For each sleeping thread, the function decreases it's sleep duration by 1.
 * If sleep duration reaches 0 and the thread isn't blocked, it will move to the ready deque.
 */
static void update_sleeping_threads() {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (data.threads[i] == nullptr) {
            continue;
        }
        if (data.threads[i]->get_is_sleeping()) {
            data.threads[i]->decrease_sleep_duration();
            if (data.threads[i]->get_sleep_duration() == 0) {
                data.threads[i]->set_is_sleeping(false);
                if (data.threads[i]->get_state() != BLOCKED) {
                    data.threads[i]->set_state(READY);
                    data.ready_threads.push_back(data.threads[i]);
                }
            }
        }
    }
}

/*
 * Manages threads when SIGVTALRM activates.
 */
static void signal_handler(int) {
    int direct_ret = sigsetjmp(data.env[data.running_thread->get_id()], 1);
    if (direct_ret == 0) {
        proc_mask(SIG_BLOCK);
        update_sleeping_threads();
        preempt();
        ++data.total_quantums;
        if (setitimer(ITIMER_VIRTUAL, &data.timer, nullptr)) {
            print_error("setitimer error.", SYSTEM);
        }
        proc_mask(SIG_UNBLOCK);
        siglongjmp(data.env[data.running_thread->get_id()], 7);
    }
}


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING.
 * There is no need to provide an entry_point or to create a stack for the main thread -
 * it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function,
 * and that it is called exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        print_error("Non-positive interval time.", LIBRARY);
        return ERROR;
    }
    if (sigemptyset(&data.sa.sa_mask) == ERROR) {
        print_error(EMPTY_SET_ERR, SYSTEM);
    }
    if (sigaddset(&data.sa.sa_mask, SIGVTALRM) == ERROR) {
        print_error(ADD_SET_ERR, SYSTEM);
    }
    data.sa.sa_handler = &signal_handler;
    if (sigaction(SIGVTALRM, &data.sa, nullptr) == ERROR) {
        print_error(ACTION_ERR, SYSTEM);
    }

    Thread *main = new Thread(MAIN_ID, RUNNING);
    data.running_thread = main;
    data.threads[MAIN_ID] = main;
    main->increase_quantums();
    setup_thread(main, nullptr);

    data.timer.it_value.tv_sec = quantum_usecs / MICRO_TO_SECS;
    data.timer.it_value.tv_usec = quantum_usecs % MICRO_TO_SECS;
    data.timer.it_interval.tv_sec = quantum_usecs / MICRO_TO_SECS;
    data.timer.it_interval.tv_usec = quantum_usecs % MICRO_TO_SECS;
    ++data.total_quantums;
    if (setitimer(ITIMER_VIRTUAL, &data.timer, nullptr)) {
        print_error("setitimer error.", SYSTEM);
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads
 * to exceed the limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) {
    proc_mask(SIG_BLOCK);
    if (Thread::count == MAX_THREAD_NUM || entry_point == nullptr) {
        print_error("Max threads number reached or null entry_point was given.", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return ERROR;
    }

    int id = produce_id();
    Thread *thread = new Thread(id, READY);
    data.threads[id] = thread;
    data.ready_threads.push_back(thread);

    setup_thread(thread, entry_point);
    proc_mask(SIG_UNBLOCK);
    return thread->get_id();
}

/*
 * Deletes the thread with the given id, and adds it's id to the min heap.
 */
static void terminate_thread(int tid) {
    delete data.threads[tid];
    data.threads[tid] = nullptr;
    data.unused_ids.push(tid);
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released.
 * If no thread with ID tid exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using exit(0) (after
 * releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise.
 * If a thread terminates itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    proc_mask(SIG_BLOCK);
    if (tid < 0 || tid >= MAX_THREAD_NUM || data.threads[tid] == nullptr) {
        print_error("bad tid to terminate.", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return ERROR;
    }
    if (tid == MAIN_ID) {
        for (int i = 0; i < MAX_THREAD_NUM; ++i) {
            terminate_thread(i);
        }
        proc_mask(SIG_UNBLOCK);
        exit(EXIT_SUCCESS);
    }

    if (tid == data.running_thread->get_id()) {
        update_sleeping_threads();
        preempt();
        data.ready_threads.pop_back();
        terminate_thread(tid);
        ++data.total_quantums;
        proc_mask(SIG_UNBLOCK);
        if (setitimer(ITIMER_VIRTUAL, &data.timer, nullptr)) {
            print_error("setitimer error.", SYSTEM);
        }
        siglongjmp(data.env[data.running_thread->get_id()], 7);
    }

    auto it = std::find(data.ready_threads.begin(), data.ready_threads.end(),
                        data.threads[tid]);
    if (it != data.ready_threads.end()) {
        data.ready_threads.erase(it);
        terminate_thread(tid);
    } else if (data.threads[tid]->get_state() == BLOCKED || data.threads[tid]->get_is_sleeping()) {
        terminate_thread(tid);
    }

    proc_mask(SIG_UNBLOCK);
    return EXIT_SUCCESS;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error
 * to try blocking the main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    proc_mask(SIG_BLOCK);

    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        print_error("illegal tid to block", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return -1;
    }

    // Main Thread
    if (tid == 0) {
        print_error("Should not block the main thread", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return -1;
    }

    // Thread exists?
    if (data.threads[tid] == nullptr) {
        print_error("There is no thread with the given id.", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return -1;
    }

    // Running Thread block itself
    if (data.running_thread->get_id() == tid) {
        data.running_thread->set_state(BLOCKED);
        proc_mask(SIG_UNBLOCK);
        signal_handler(SIGVTALRM);
        return 0;
    }

    // Blocking not running thread
    if (data.threads[tid]->get_state() == READY) {
        data.threads[tid]->set_state(BLOCKED);
        auto it = std::find(data.ready_threads.begin(), data.ready_threads.end(),
                            data.threads[tid]);
        if (it != data.ready_threads.end()) {
            data.ready_threads.erase(it);
        }
    }

    if (data.threads[tid]->get_is_sleeping()) {
        data.threads[tid]->set_state(BLOCKED);
    }


    proc_mask(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error.
 * If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    proc_mask(SIG_BLOCK);

    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        print_error("illegal tid to resume", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return -1;
    }

    if (data.threads[tid] == nullptr) {
        print_error("There is no thread with the given id.", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return -1;
    }

    if (data.threads[tid]->get_state() == BLOCKED && !data.threads[tid]->get_is_sleeping()) {
        data.threads[tid]->set_state(READY);
        data.ready_threads.push_back(data.threads[tid]);
    }
    if (data.threads[tid]->get_is_sleeping()) {
        data.threads[tid]->set_state(SLEEP);
    }

    proc_mask(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision
 * should be made. After the sleeping time is over, the thread should go back to the end of the
 * READY queue. If the thread which was just RUNNING should also be added to the READY queue,
 * or if multiple threads wake up at the same time, the order in which they're added to the end
 * of the READY queue doesn't matter. The number of quantums refers to the number of times a
 * new quantum starts, regardless of the reason. Specifically, the quantum of the thread which
 * has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    proc_mask(SIG_BLOCK);

    if (num_quantums <= 0) {
        print_error("num_quantums must be positive", LIBRARY);
        return -1;
    }

    if (data.running_thread->get_id() == MAIN_ID) {
        print_error("it's illegal to sleep the main thread", LIBRARY);
        return -1;
    }

    data.running_thread->set_sleep_duration(num_quantums + 1);
    data.running_thread->set_is_sleeping(true);
    data.running_thread->set_state(SLEEP);

    proc_mask(SIG_UNBLOCK);
    signal_handler(0);

    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return data.running_thread->get_id();
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the
 * current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return data.total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that
 * the thread starts should increase this value by 1 (so if the thread with ID tid is in RUNNING
 * state when this function is called, include also the current quantum).
 * If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid.
 *         On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    proc_mask(SIG_BLOCK);
    if (tid < 0 || tid >= MAX_THREAD_NUM || data.threads[tid] == nullptr) {
        print_error("bad tid to get quantums.", LIBRARY);
        proc_mask(SIG_UNBLOCK);
        return ERROR;
    }
    proc_mask(SIG_UNBLOCK);
    return data.threads[tid]->get_quantums();
}
