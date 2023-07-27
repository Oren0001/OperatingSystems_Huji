#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "../uthreads.h"
#include <sys/time.h>
#include "cstring"
#include <complex>
#include "cmath"

int flag = 0;


/****** instructions **/
// 1) every test includes test and function(s) related to this test.
// 2) you have to run every test separately.
// 3) if you want to check if the timer signal is blocked, use the
//    function check_block_timer(char* str) with a string to print in case it's
//    blocked.
// 4) in some tests we used memcopy, this was used for taking up running time
//    and is not relevent to the tests.
// 5) in most of the tests running of the main thread prints numbers 1...100000




void check_block_timer(char* str);

/**
 * this function used for checking if the timer was blocked without unblocking
 * @param str
 */
void check_block_timer(char* str) {
    sigset_t  empty;
    sigemptyset(&empty);
    sigprocmask(SIG_UNBLOCK,NULL,&empty);
    if (sigismember(&empty, SIGVTALRM)==1){
        printf("time_blocked %s",str);
    }
}

/**
 * function for checking block and resume
 * @return
 */
int func_regular_block_resume(){
    for (int i =0;i<100000;++i){
        printf("regular_block_resume\n");
    }
    flag =1;
    printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
    uthread_terminate(uthread_get_tid());
}


/**
 * function which tests a regular sleep
 * @return
 */
int func_sleep(){
    uthread_sleep(15);
    for (int i =0;i<1000;++i){
        printf("func_sleep\n");
    }
    flag = 1;
    printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
    uthread_terminate(uthread_get_tid());
}
/**
 * function for block and sleep
 * @return
 */
int func_block_and_sleep(){
    uthread_sleep(1);
    printf("sleeping\n");
    for (int i =0;i<10000;++i){
        printf("block_and_sleep\n");
    }
    flag = 1;
    printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
    uthread_terminate(uthread_get_tid());
}
/**
 * function for sleep and block

 * @return
 */
int func_sleep_and_block() {
    uthread_sleep(15);
    printf("sleeping\n");
    for (int i = 0; i < 10000; ++i) {
        printf("sleep_block\n");
    }
        flag = 1;
        printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
        uthread_terminate(uthread_get_tid());
        return 1;
}

/**
 * function for threead the block itself
 * @return
 */
int func_block_itself(){
    for (int i =0;i<10000;++i){
        if (i==10){
            uthread_block(1);
        }
        printf("block_itself\n");
    }

    flag =1;
    printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
    uthread_terminate(uthread_get_tid());
    return 1;
}
/**
 * function for threead the terminate itself
 * @return
 */
int func_terminate_itself(){
    for (int i =0;i<10000;++i){
        if (i==10){
            uthread_terminate(1);
        }
        printf("terminate_itself\n");
    }
    flag =1;
    return 1;
}
/**
 * this function checks situation that thread block itself
 * in a successful run you will see:
 * 1) print "main running: %d\n",uthread_get_quantums(0));
 * 2) print "block_itself" in range (1,10)
 * 3) print "thread num %d = %d\n", 1, uthread_get_quantums(1));
 * 4) print "main running: %d\n",uthread_get_quantums(0));
 * 5) print "thread num %d = %d\n", 0, uthread_get_quantums(0)
   6) print "total = %d\n", uthread_get_total_quantums()

 * @return
 */
int test_thread_block_itself(){
    flag = 0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_block_itself));
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);
    for (int i=0;i<100000;i++){
        printf("main running: %d\n",uthread_get_quantums(0));
        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }
    free(temp);
    free(temp2);
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf("total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;
}
/**
 * this function checks situation that thread terminate itself
 * in a successful run you will see:
 * 1) print "main running: %d\n",uthread_get_quantums(0));
 * 2) print "terminate_itself" in range (1,10)
 * 3) print "main running: %d\n",uthread_get_quantums(0));
 * 4) print "thread num %d = %d\n", 0, uthread_get_quantums(0)
   5) print "total = %d\n", uthread_get_total_quantums()

 *
 */
int test_thread_terminate_itself(){
    flag = 0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_terminate_itself));
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);
    for (int i=0;i<100000;i++){
        printf("main running: %d\n",uthread_get_quantums(0));

        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }

    free(temp);
    free(temp2);
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf("total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;

}
/**
 * this function test sleep and block
 *  in a successful run you will see:
 * 1) print "main running: %d\n",uthread_get_quantums(0));
 * 2) print "sleeping"
 * 3) print "main running: %d\n",uthread_get_quantums(0));
 * 4) print "blocked"
 * 5) print "main running: %d\n",uthread_get_quantums(0));
 * 6) print "resumed"
 * 7) print "sleep_block" in range 10000
 * 8) print "thread num %d = %d\n", 1, uthread_get_quantums(0)
 * 9) print "main running: %d\n",uthread_get_quantums(0));
 * 10) print "thread num %d = %d\n", 0, uthread_get_quantums(0)
   11) print "total = %d\n", uthread_get_total_quantums()
 * @return
 */
int test_sleep_and_block(){
    flag = 0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_sleep_and_block));
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);
    for (int i=0;i<1000000;i++){
        if (i==10000){
            printf("blocked\n");
            uthread_block(1);
        }
        if (i==100000){
            uthread_resume(1);
            printf("resumed\n");
        }
        printf("main running: %d\n",uthread_get_quantums(0));
        memcpy(temp,temp2,1000);
    }
    while (flag != 1){
        printf("main running: %d\n",uthread_get_quantums(0));
    }
    flag = 0;
    free(temp);
    free(temp2);
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf(" total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;


}
/**
 * this function block thread and sleep the thread
 * @expected: thread 1 prints only after returning from sleep, use debugger
 * to verify
 */
int test_block_and_sleep(){
    flag = 0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_block_and_sleep));
    printf("sleeping\n");

    // copy memory, just to spend time
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);

    for (int i=0;i<100000;i++){
        if (i==25000){
            uthread_block(1);
            printf("blocked\n");
        }
        if (i==99999){
            uthread_resume(1);
        }
        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }
    while (flag != 1){
    }
    flag =0;
    free(temp);
    free(temp2);
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf(" total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;


}
/**
 * tests regular sleep
 * @expected: sleeping thread should start printing only after 15 quants
 */
int test_sleep(){
    flag = 0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_sleep));
    printf("sleeping\n");

    // copy memory, just to spend time
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);

    for (int i=0;i<10000;i++){
        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }
    while (flag != 1){
    }
    printf("finished sleep\n");
    flag = 0;
    free(temp);
    free(temp2);
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf(" total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;
}
/**
 * this function test regular blocking and resuming
 * @expected: The main thread should print numbers up until 300 without
 * prints from the blocked thread
 */
int test_regular_block_resume(){
    flag =0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_regular_block_resume));
    uthread_block(1);
    printf("blocked\n");
    int val =0;
    void* temp = malloc(1000);
     void* temp2 = malloc(1000);

    for (int i=0;i<100000;i++){
        if (i==300){
            uthread_resume(1);
            printf("unblock\n");
        }
        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }
    free(temp);
    free(temp2);
    while (flag!=1){
    }
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf(" total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);

    return 1;
}
int func1(){
    for (int i=0;i<100000;i++){
        printf("func1\n");
    }
    uthread_terminate(1);
}
int func2(){
    for (int i=0;i<100000;i++){
        printf("func2\n");
    }
    uthread_terminate(2);
}
int func3(){
    for (int i=0;i<100000;i++){
        printf("func3\n");
    }

    uthread_terminate(3);
}
int func4(){
    for (int i=0;i<100000;i++){
        printf("func4\n");
    }
    flag =1;
    uthread_terminate(4);
}

/**
 * this function checks switch of 4 threads. you have to see switching of func1,
 * func2, func3, func4.
 * @expected: alternating prints
 */
int test_switch_4_threads(){
    flag =0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func1));
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func2));
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func3));
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func4));
    while (flag!=1){
    }
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf(" total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
}
int func_switch_thread(){
    for (int i =0;i<100000;++i){
        printf("switch thread\n");
    }//
    flag =1;
    printf("thread num %d = %d\n", 1, uthread_get_quantums(1));
    uthread_terminate(uthread_get_tid());

}
/**
 * tests regular switch, between the main and another thread
 *in the main thread you have to see numbers between 1.....100000
 * in thread 2 there is a switch thread print
 * @expected: alternating prints
 *
 */
int test_switch_thread(){
    flag =0;
    uthread_init(1);
    uthread_spawn(reinterpret_cast<thread_entry_point>(&func_switch_thread));
    void* temp = malloc(1000);
    void* temp2 = malloc(1000);

    for (int i=0;i<100000;i++){
        printf("%d\n",i);
        memcpy(temp,temp2,1000);
    }
    free(temp);
    free(temp2);
    while (flag!=1){
    }
    printf("thread num %d = %d\n", 0, uthread_get_quantums(0));
    printf("total = %d\n", uthread_get_total_quantums());
    uthread_terminate(0);
    return 1;

}
int main(int argc ,char *argv[]){
//    test_switch_thread();
//    test_switch_4_threads();
//    test_regular_block_resume();
//    test_sleep();
//    test_block_and_sleep();
//    test_sleep_and_block();
//    test_thread_block_itself();
    test_thread_terminate_itself();
    return 0;
}

