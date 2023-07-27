#include "osm.h"
#include <sys/time.h>

#define ERROR -1
#define UNROLLING_FACTOR 5
#define TO_NANO 1e9
#define MICRO_TO_NANO 1e3


/*
 * Calculates the average time an operation took and returns it.
 */
double calculate_avg_time(timeval &start, timeval &end, unsigned int &iterations) {
    double sec = (double) end.tv_sec - start.tv_sec;
    double micro_sec = (double) end.tv_usec - start.tv_usec;
    double time = sec * TO_NANO + micro_sec * MICRO_TO_NANO;
    return time / iterations;
}

/*
 * Rounds UP the number of iterations to a multiple of the unrolling factor.
 */
void round_up_iterations(unsigned int &iterations) {
    unsigned int r = iterations % UNROLLING_FACTOR;
    if (r != 0) {
        iterations += UNROLLING_FACTOR - r;
    }
}


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations) {
    if (iterations != 0) {
        round_up_iterations(iterations);
        struct timeval start, end;
        int a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0;
        if (gettimeofday(&start, nullptr) != ERROR) {
            for (unsigned int i = 0; i < iterations; i += UNROLLING_FACTOR) {
                a0 = a0 + 1;
                a1 = a1 + 1;
                a2 = a2 + 1;
                a3 = a3 + 1;
                a4 = a4 + 1;
            }
            if (gettimeofday(&end, nullptr) != ERROR) {
                return calculate_avg_time(start, end, iterations);
            }
        }
    }
    return ERROR;
}

void empty_function() {}

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations) {
    if (iterations != 0) {
        round_up_iterations(iterations);
        struct timeval start, end;
        if (gettimeofday(&start, nullptr) != ERROR) {
            for (unsigned int i = 0; i < iterations; i += UNROLLING_FACTOR) {
                empty_function();
                empty_function();
                empty_function();
                empty_function();
                empty_function();
            }
            if (gettimeofday(&end, nullptr) != ERROR) {
                return calculate_avg_time(start, end, iterations);
            }
        }
    }
    return ERROR;
}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations) {
    if (iterations != 0) {
        round_up_iterations(iterations);
        struct timeval start, end;
        if (gettimeofday(&start, nullptr) != ERROR) {
            for (unsigned int i = 0; i < iterations; i += UNROLLING_FACTOR) {
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
            }
            if (gettimeofday(&end, nullptr) != ERROR) {
                return calculate_avg_time(start, end, iterations);
            }
        }
    }
    return ERROR;
}
