/* CS519, Spring 2026: Project 2
   Written by: Arthur Levitsky and Alexander Wu
   Description: Benchmarking multi-threaded vector addition in an infinite loop.
*/

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

#include "../headers/benchmark.h"

#define NUM_THREADS sysconf(_SC_NPROCESSORS_CONF)
#define THREAD_MULTIPLIER 2
#define VECTOR_SIZE 1000
#define SYS_SET_INACTIVE 449

// Cooperative mode toggle for comparison in reports
static int coop_mode = 0;

// Syscall wrapper for set_inactive
static inline long set_inactive(int is_inactive)
{
    return syscall(SYS_SET_INACTIVE, is_inactive);
}

/* Test Portion*/
static volatile sig_atomic_t running = 1;

typedef struct {
    long iterations;
} thread_data_t;

void handle_sigint(int sig);
void *vector_add_loop(void *arg);
void arg_check(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    arg_check(argc, argv);

    if (argc >= 2 && strcmp(argv[1], "coop") == 0) {
        coop_mode = 1;
    }

    long online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    long num_threads = THREAD_MULTIPLIER * online_cpus;

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
    printf("Cooperative mode: %s\n", coop_mode ? "enabled" : "disabled");
    printf("Press Ctrl+C to stop...\n\n");

    signal(SIGINT, handle_sigint);
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (long i = 0; i < num_threads; i++) {
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

    for (long i = 0; i < num_threads; i++) {
        long iters = thread_args[i].iterations;
        total_iters += iters;

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

    free(threads);
    free(thread_args);
    return 0;
}

void arg_check(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("Usage: %s [normal|coop]\n", argv[0]);
        printf("------------------------------------------------------\n");
        printf("normal - run background spinner normally\n");
        printf("coop   - mark spinner threads inactive using set_inactive\n");
        exit(1);
    }

    if (strcmp(argv[1], "coop") != 0 && strcmp(argv[1], "normal") != 0) {
        printf("Invalid mode. Use 'normal' or 'coop'.\n");
        exit(1);
    }
}

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void *vector_add_loop(void *arg) {
    thread_data_t *d = (thread_data_t *)arg;

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
        sink += C[d->iterations % VECTOR_SIZE];
        d->iterations++;
    }

    (void)sink;

    if (coop_mode) {
        set_inactive(0);
    }

    return NULL;
}

/**
typedef struct {
    int    *A;
    int    *B;
    int    *C;
    size_t  start;       
    size_t  end;         
    long    iterations;
} thread_data_t;

// Threads stop when this is cleared by SIGINT.
static volatile int running = 1;

void handle_sigint(int sig);
void *vector_add_loop(void *arg);
void arg_check(int argc, char *argv[]);
double get_total_time_ms(struct timespec start, struct timespec end);

int main(int argc, char *argv[]) {
    arg_check(argc, argv);
    // Check if coop is requested
    if (argc >= 3 && strcmp(argv[2], "coop") == 0) {
        coop_mode = 1;
    }

    long vector_size = atol(argv[1]);
    long num_threads = THREAD_MULTIPLIER * NUM_THREADS;
    long elems_per_thread = vector_size / num_threads;

    // Allocate vectors. Allocate C vector and initialize with 0s.
    int *A = malloc(vector_size * sizeof(int));
    int *B = malloc(vector_size * sizeof(int));
    int *C = calloc(vector_size,  sizeof(int));

    if (A == NULL || B == NULL || C == NULL) {
        fprintf(stderr, "Allocation for vectors A, B, C failed. Exiting...");
        return 1; 
    }

    // Initialize A and B with random values
    srand((unsigned)time(NULL));
    for (long i = 0; i < vector_size; i++) {
        A[i] = rand();
        B[i] = rand();
    }

    pthread_t threads[num_threads];
    thread_data_t thread_args[num_threads];
    struct timespec t_start, t_end;

    printf("Number of threads: %ld\n", num_threads);
    printf("Thread multiplier: %d\n", THREAD_MULTIPLIER);
    printf("Vector size: %ld\n", vector_size);
    printf("Elements per thread: %ld\n", elems_per_thread);
    printf("Cooperative mode: %s\n", coop_mode ? "enabled" : "disabled");
    printf("Press Ctrl+C to stop...\n\n");

    signal(SIGINT, handle_sigint);  
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int i = 0; i < num_threads; i++) {
        thread_args[i].A     = A;
        thread_args[i].B     = B;
        thread_args[i].C     = C;
        thread_args[i].start = (size_t)(i * elems_per_thread);

        // Last thread handles any remainders
        thread_args[i].end   = (i == num_threads - 1)
                                ? (size_t)vector_size
                                : (size_t)((i + 1) * elems_per_thread);
        pthread_create(&threads[i], NULL, vector_add_loop, &thread_args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    // Aggregate stats
    long full_passes = thread_args[0].iterations;
    long total_iters = 0;
    for (int i = 0; i < num_threads; i++) {
        total_iters += thread_args[i].iterations;
    }

    double elapsed_sec = get_total_time(t_start, t_end);

    printf("\n\nFull vector passes (thread 0): %ld\n", full_passes);
    printf("Total time (sec): %.2f\n",  elapsed_sec);
    printf("Total iterations (all threads): %ld\n",   total_iters);
    printf("Throughput (iter/sec): %.2f\n",  total_iters / elapsed_sec);

    free(A);
    free(B);
    free(C);
    return 0;
}

void arg_check(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("Usage: %s [VECTOR_SIZE] [mode]\n", argv[0]);
        printf("------------------------------------------------------\n");
        printf("VECTOR_SIZE - number of integer elements (2 - 10000)\n");
        printf("mode        - optional: coop enables cooperative scheduling\n");
        exit(1);
    }

    if (atoi(argv[1]) < 2 || atoi(argv[1]) > 10000) {
        printf("Incompatible VECTOR_SIZE! (2 - 10000)\n");
        exit(1);
    }
    if (argc >= 3 && strcmp(argv[2], "coop") != 0 && strcmp(argv[2], "normal") != 0) {
        printf("Invalid mode. Use 'normal' or 'coop'.\n");
        exit(1);
    }
}


// When pressing CTRL+C to end program, execute this function.
void handle_sigint(int sig) { 
    (void)sig; 
    running = 0; 
}

// Infinite vector addition of A + B = C sliced for each thread.
void *vector_add_loop(void *arg) {
    thread_data_t *d = (thread_data_t *)arg;
    d->iterations = 0;

    if (coop_mode) {
        long ret = set_inactive(1);
        if (ret != 0) {
            perror("set_inactive");
            running = 0;
            return NULL;
        }
    }

    while (running) {
        for (size_t i = d->start; i < d->end; i++) {
            d->C[i] = d->A[i] + d->B[i];
        }

        d->iterations++;
    }

    if (coop_mode) {
        set_inactive(0);
    }

    return NULL;
}*/