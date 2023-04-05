/* CSE 536: User-Level Threading Library */
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "user/ulthread.h"

/* Standard definitions */
#include <stdbool.h>
#include <stddef.h> 

struct uthread_def  uthread_arr[MAXULTHREADS];
struct uthread_def *current_thread;
int uthread_scheduling_algorithm = -1;

int created_threads = 0;

int prev_tid = 0;

/* Get thread ID */
int get_current_tid(void) {
    return current_thread->tid;
}

/* Thread initialization */
void ulthread_init(int schedalgo) {

    /* Add this statement to denote which scheduling algorithm is being used */
    printf("[*] ultinit(schedalgo: %d)\n", schedalgo);

    uthread_arr[0].state = RUNNABLE;
    uthread_arr[0].tid = 0;
    current_thread = &uthread_arr[0];

    uthread_scheduling_algorithm = schedalgo;

    memset(&uthread_arr[0].context, 0, sizeof(struct context_def));

    for (int i = 1; i < MAXULTHREADS; i++) {
        uthread_arr[0].priority = -1;
        uthread_arr[0].sched_time = -1;
        uthread_arr[0].state = FREE;
    }


}

/* Thread creation */
bool ulthread_create(uint64 start, uint64 stack, uint64 args[], int priority) {

    for (int i = 1; i < MAXULTHREADS; i++) {
        if (uthread_arr[i].state == FREE) {

            uthread_arr[i].state = RUNNABLE;
            uthread_arr[i].tid = i;
            uthread_arr[i].priority = priority;
            

            memset(&(uthread_arr[i].context), 0, sizeof(struct context_def));
            uthread_arr[i].context.ra = start;
            uthread_arr[i].context.sp = stack;

            uthread_arr[i].context.s0 = args[0];
            uthread_arr[i].context.s1 = args[1];
            uthread_arr[i].context.s2 = args[2];
            uthread_arr[i].context.s3 = args[3];
            uthread_arr[i].context.s4 = args[4];
            uthread_arr[i].context.s5 = args[5];

            // uthread_arr[i].sched_time = ctime();
            // printf("%d has sched time %d",  i, uthread_arr[i].sched_time);

            created_threads++;

            printf("[*] ultcreate(tid: %d, ra: %p, sp: %p)\n", i, start, stack);

            return true;
        }
    }

    return false;
}

/* find next thread */

int find_next_thread(void) {
    int next_thread = -1;
    int min_sched_time = 100000;
    int min_priority = -1;

    for (int i = prev_tid + 1; i <= MAXULTHREADS + prev_tid; i++) {
        int idx = i % MAXULTHREADS;
        if ((prev_tid != &(uthread_arr[idx]).tid) && (uthread_arr[idx].state == RUNNABLE || uthread_arr[idx].state == YIELD)) {
            if (uthread_arr[idx].state == YIELD) {
                // printf("Created Threads: %d\n", created_threads);
                uthread_arr[idx].state = RUNNABLE;
                if (created_threads != 1) {
                    continue;
                }
            }
            if (uthread_scheduling_algorithm == 0) {
                // printf("%d has sched time %d. Min: %d", i, uthread_arr[i].sched_time, min_sched_time);
                if (uthread_arr[idx].sched_time < min_sched_time) {
                    min_sched_time = uthread_arr[idx].sched_time;
                    next_thread = idx;
                }
            } else if (uthread_scheduling_algorithm == 1) {
                // printf("%d has priority %d. Min: %d\n", i, uthread_arr[i].priority, min_priority);
                if (uthread_arr[idx].priority > min_priority) {
                    min_priority = uthread_arr[idx].priority;
                    next_thread = idx;
                }
            }
        }
    }

    return next_thread;
}

/* Thread scheduler */
void ulthread_schedule(void) {
    
    /* Add this statement to denote which thread-id is being scheduled next */

    for (int i = 0; i < MAXULTHREADS; i++) {
        int temp_tid = current_thread->tid;
        int next_thread = find_next_thread();

        if (next_thread == -1) {
            break;
        }

        printf("[*] ultschedule (next tid: %d)\n", next_thread);

        current_thread = &(uthread_arr[next_thread]);

        ulthread_context_switch(&(uthread_arr[temp_tid].context), &(uthread_arr[next_thread].context));

        // printf("BACK FROM YIELD!");

    }

    // printf("thread %d is done\n", temp_tid);

}

/* Yield CPU time to some other thread. */
void ulthread_yield(void) {

    if (created_threads == 0) {
        printf("No more threads to run. Exiting...\n");
        exit(0);
    }

    int temp_tid = current_thread->tid;
    prev_tid = current_thread->tid;
    int next_thread_tid = 0;

    uthread_arr[temp_tid].state = YIELD;

    current_thread = &(uthread_arr[next_thread_tid]);

    /* Please add thread-id instead of '0' here. */
    printf("[*] ultyield(tid: %d)\n", temp_tid);

    // printf("YIELD!");

    ulthread_context_switch(&(uthread_arr[temp_tid].context), &(uthread_arr[next_thread_tid].context));

}

/* Destroy thread */
void ulthread_destroy(void) {
    printf("[*] ultdestroy(tid: %d)\n", current_thread->tid);
    current_thread->state = FREE;
    current_thread->tid = -1;

    created_threads--;

    struct uthread_def *temp = current_thread;
    struct uthread_def *next = &uthread_arr[0];

    current_thread = next;

    ulthread_context_switch(&(temp->context), &(next->context));

}
