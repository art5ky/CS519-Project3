/* CS519, Spring 2026: Project 2
   Written by: Arthur Levitsky and Alexander Wu
   Description: Benchmarking multi-threaded vector addition in an infinite loop.
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sched.h>

#include "../headers/benchmark.h"

#define NUM_THREADS sysconf(_SC_NPROCESSORS_CONF)
#define THREAD_MULTIPLIER 2
#define VECTOR_SIZE 1000
#define SYS_SET_INACTIVE 449

// Cooperative mode toggle for comparison in reports
static int coop_mode = 0;
// Toggle for yield mode for comparison in reports
static int yield_mode = 0;

// Syscall wrapper for set_inactive
static inline long set_inactive(int is_inactive)
{
    return syscall(SYS_SET_INACTIVE, is_inactive);
}

/* Test Portion*/
static volatile sig_atomic_t running = 1;

typedef struct {
    long iterations;
    long involuntary_switches;
    double cpu_time_ms;
    int cpu_id;
} thread_data_t;

void handle_sigint(int sig);
void *vector_add_loop(void *arg);
void arg_check(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    arg_check(argc, argv);

    if (strcmp(argv[1], "coop") == 0) {
        coop_mode = 1;
    } else if (strcmp(argv[1], "yield") == 0) {
        yield_mode = 1;
    }

    const char *mode_str = coop_mode ? "cooperative" :
                            yield_mode ? "yield" : "normal";

    long online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    long num_threads = THREAD_MULTIPLIER * online_cpus;

    // Override num_threads with custom number if specified by user input
    if (argc >= 3) {
        long override = atol(argv[2]);
        if (override > 0) {
            num_threads = override;
        }
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_data_t *thread_args = calloc(num_threads, sizeof(thread_data_t));

    if (threads == NULL || thread_args == NULL) {
        fprintf(stderr, "Allocation for thread metadata failed. Exiting...\n");
        free(threads);
        free(thread_args);
        return 1;
    }

    struct timespec t_start, t_end;

    printf("Number of CPUs: %ld\n", online_cpus);
    printf("Number of threads: %ld\n", num_threads);
    printf("Thread multiplier: %d\n", THREAD_MULTIPLIER);
    printf("Per-thread vector size: %d\n", VECTOR_SIZE);
    printf("Mode: %s\n", mode_str);
    printf("Press Ctrl+C to stop...\n\n");

    signal(SIGINT, handle_sigint);
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (long i = 0; i < num_threads; i++) {
        thread_args[i].cpu_id = i % online_cpus;
        if (pthread_create(&threads[i], NULL, vector_add_loop, &thread_args[i]) != 0) {
            perror("pthread_create");
            running = 0;
            num_threads = i;
            break;
        }
    }

    for (long i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    long total_iters = 0;
    long min_iters = thread_args[0].iterations;
    long max_iters = thread_args[0].iterations;
    long total_involuntary = 0;
    double total_cpu_ms = 0.0;

    for (long i = 0; i < num_threads; i++) {
        long iters = thread_args[i].iterations;
        total_iters += iters;
        total_involuntary += thread_args[i].involuntary_switches;
        total_cpu_ms += thread_args[i].cpu_time_ms;

        if (iters < min_iters)
            min_iters = iters;
        if (iters > max_iters)
            max_iters = iters;
    }

    double elapsed_sec = get_total_time(t_start, t_end);

    printf("\nTotal time (sec): %.2f\n", elapsed_sec);
    printf("Total iterations (all threads): %ld\n", total_iters);
    printf("Throughput (iter/sec): %.2f\n", total_iters / elapsed_sec);
    printf("Min thread iterations: %ld\n", min_iters);
    printf("Max thread iterations: %ld\n", max_iters);
    printf("Total CPU time (ms): %.2f\n", total_cpu_ms);
    printf("Involuntary context switches: %ld\n", total_involuntary);

    free(threads);
    free(thread_args);
    return 0;
}

void arg_check(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("Usage: %s [normal|coop|yield]\n", argv[0]);
        printf("------------------------------------------------------\n");
        printf("normal - run background spinner normally\n");
        printf("coop   - mark spinner threads inactive using set_inactive\n");
        printf("yield  - call sched_yield() each iteration\n");
        printf("num_threads - optional, defaults to 2 * online CPUs\n");
        exit(1);
    }

    if (strcmp(argv[1], "coop") != 0 && strcmp(argv[1], "normal") != 0 && strcmp(argv[1], "yield") != 0) {
        printf("Invalid mode. Use 'normal', 'coop', or 'yield'.\n");
        exit(1);
    }
}

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void *vector_add_loop(void *arg) {
    thread_data_t *d = (thread_data_t *)arg;

    // Pin this thread to assigned CPU so it doesn't move work to others
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(d->cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    volatile float A[VECTOR_SIZE];
    volatile float B[VECTOR_SIZE];
    volatile float C[VECTOR_SIZE];

    for (int i = 0; i < VECTOR_SIZE; i++) {
        A[i] = (float)i;
        B[i] = (float)(VECTOR_SIZE - i);
        C[i] = 0.0f;
    }

    d->iterations = 0;

    if (coop_mode) {
        long ret = set_inactive(1);
        if (ret != 0) {
            perror("set_inactive");
            running = 0;
            return NULL;
        }
    }

    volatile float sink = 0.0f;

    while (running) {
        for (int i = 0; i < VECTOR_SIZE; i++) {
            C[i] = A[i] + B[i];
        }
        sink += C[d->iterations % VECTOR_SIZE]; // Dummy unused value to prevent copmpiler from complaining
                                                // about unused C[i]
        d->iterations++;

        if (yield_mode) {
            sched_yield();
        }
    }

    (void)sink;

    if (coop_mode) {
        set_inactive(0);
    }

    struct rusage usage;
    getrusage(RUSAGE_THREAD, &usage);

    d->involuntary_switches = usage.ru_nivcsw;

    d->cpu_time_ms =
        usage.ru_utime.tv_sec * 1000.0 + usage.ru_utime.tv_usec / 1000.0 +
        usage.ru_stime.tv_sec * 1000.0 + usage.ru_stime.tv_usec / 1000.0;

    return NULL;
}
