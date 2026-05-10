/*  CS519, Spring 2026: Project 3
    Written by: Arthur Levitsky and Alexander Wu
    Description: IPC using shared memory to perform matrix multiplication.
*/
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sched.h>

#include "../headers/sqmatrix.h"
#include "../headers/benchmark.h"
#include "../headers/locks.h"

void safe_read(int fd, void *buf, size_t count);
void arg_check(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    int MATRIX_SIZE;
    arg_check(argc, argv);
    MATRIX_SIZE = atoi(argv[1]);

    srand(time(NULL));
    struct timespec start, end; 

    long num_workers = sysconf(_SC_NPROCESSORS_ONLN);
    int base_rows = MATRIX_SIZE / num_workers; 
    int remainder = MATRIX_SIZE % num_workers;
    int start_row, end_row; 

    long total_involuntary_switches = 0;
    
    // unlike with pipes, the matrices have been allocated and delivered into a shared space.
    int **A = malloc_sq_matrix(MATRIX_SIZE);
    int **B = malloc_sq_matrix(MATRIX_SIZE);
    int **B_T = malloc_sq_matrix(MATRIX_SIZE); 
    int **C = malloc_sq_matrix_shared(MATRIX_SIZE);

    if (A == NULL || B == NULL || C == NULL || B_T == NULL) {
        fprintf(stderr, "Allocation for matrices A, B, B_T, C failed. Exiting...");
        return 1; 
    }
    
    rand_init_sq_matrix(A, MATRIX_SIZE);
    rand_init_sq_matrix(B, MATRIX_SIZE);
    zero_init_sq_matrix(C, MATRIX_SIZE);
    transpose_sq_matrix(B, B_T, MATRIX_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork(); 

        if (pid == -1) {
            perror("Failed creating child process!");
            exit(1); 
        }

        // Child process successfully created. Begin work...
        if (pid == 0) {
            // Pin child process to CPU
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);

            // Based on the subdivisions per child process, calculate the starting and ending rows factoring in remainders.
            start_row = i * base_rows + (i < remainder ? i : remainder);
            end_row = start_row + base_rows + (i < remainder ? 1 : 0);
            
            // Because each process is working on it's own row of the C matrix, there is no contentions therefore locks are not needed.
            for (int r = start_row; r < end_row; r++) {
                mult_sq_matrices_row_transposed(r, A, B_T, C, MATRIX_SIZE);
            }
            exit(0); 
        }
    }

    // Wait for all child processes to terminate. If something goes wrong in termination, report errors.
    for (int i = 0; i < num_workers; i++) {
        int status;
        struct rusage usage;

        pid_t child_pid = wait4(-1, &status, 0, &usage);

        if (child_pid == -1) {
            perror("wait4 failed");
            continue;
        }

        total_involuntary_switches += usage.ru_nivcsw;

        if (!WIFEXITED(status)) {
            fprintf(stderr, "Child process: %d terminated abnormally!\n", child_pid);
        } else {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                fprintf(stderr, "Child process: %d failed with code: %d\n", child_pid, exit_code);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_time_sec = get_total_time(start, end);

    // If MATRIX_SIZE <= 1000, do single process matrix multiplication for verification otherwise use Freivalds algorithm.
    bool verified = verified_matrix(A, B, C, MATRIX_SIZE);   
    print_stats(MATRIX_SIZE, num_workers, verified, total_time_sec);
    printf("Involuntary context switches: %ld\n", total_involuntary_switches);

    free_sq_matrix(A);
    free_sq_matrix(B);
    free_sq_matrix(B_T);
    free_sq_matrix_shared(C, MATRIX_SIZE);
    return 0; 
}

// Instead of using macros, I made it easier to just include arguments into the pipe program.
void arg_check(int argc, char *argv[]) {
     if (argc <= 1) {
        printf("Usage: %s [MATRIX_SIZE]\n", argv[0]);
        printf("------------------------------------------------\n");
        printf("MATRIX_SIZE - Set a matrix size      (2 - 10000)\n");
        exit(1); 
    }

    if (atoi(argv[1]) < 2 || atoi(argv[1]) > 10000) {
        printf("Incompatible MATRIX_SIZE! (2 - 10000)\n");
        exit(1); 
    }
}