#include "Barrier.h"
#include <cstdlib>
#include <iostream>

Barrier::Barrier(int numThreads) :
        mutex(PTHREAD_MUTEX_INITIALIZER),
        cv(PTHREAD_COND_INITIALIZER),
        count(0),
        numThreads(numThreads) {}


Barrier::~Barrier() {
    if (pthread_mutex_destroy(&mutex) != 0) {
        std::cout << "system error: pthread_mutex_destroy failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&cv) != 0) {
        std::cout << "system error: pthread_cond_destroy failed." << std::endl;
        exit(EXIT_FAILURE);
    }
}


void Barrier::barrier() {
    if (pthread_mutex_lock(&mutex) != 0) {
        std::cout << "system error: pthread_mutex_lock failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    if (++count < numThreads) {
        if (pthread_cond_wait(&cv, &mutex) != 0) {
            std::cout << "system error: pthread_cond_wait failed." << std::endl;
            exit(EXIT_FAILURE);
        }
    } else {
        count = 0;
        if (pthread_cond_broadcast(&cv) != 0) {
            std::cout << "system error: pthread_cond_broadcast failed." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_mutex_unlock(&mutex) != 0) {
        std::cout << "system error: pthread_mutex_unlock" << std::endl;
        exit(EXIT_FAILURE);
    }
}
