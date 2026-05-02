/*  CS519, Spring 2026: Project 1 - Part 2
    Written by: Arthur Levitsky
    Description: IPC using shared memory to perform matrix multiplication.
    No locking mechanism is needed because no contention in shared memory.
*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

#include "../headers/sqmatrix.h"
#include "../headers/benchmark.h"
#include "../headers/locks.h"

void safe_read(int fd, void *buf, size_t count);
void arg_check(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    int MATRIX_SIZE, WORKERS;
    bool USE_TRANSPOSE;

    arg_check(argc, argv);

    MATRIX_SIZE = atoi(argv[1]);
    WORKERS = atoi(argv[2]);
    USE_TRANSPOSE = (strcmp(argv[3], "true") == 0);

    srand(time(NULL));
    struct timespec start, end; 

    long num_workers = WORKERS ? WORKERS : sysconf(_SC_NPROCESSORS_ONLN);
    int base_rows = MATRIX_SIZE / num_workers; 
    int remainder = MATRIX_SIZE % num_workers;
    int start_row, end_row; 
    
    
    // unlike with pipes, the matrices have been allocated and delivered into a shared space.
    int **A = malloc_sq_matrix(MATRIX_SIZE);
    int **B = malloc_sq_matrix(MATRIX_SIZE);
    int **B_T = NULL; 
    int **C = malloc_sq_matrix_shared(MATRIX_SIZE);

    if (A == NULL || B == NULL || C == NULL) {
        fprintf(stderr, "Allocation for matrices A, B, C failed. Exiting...");
        return 1; 
    }
    
    rand_init_sq_matrix(A, MATRIX_SIZE);
    rand_init_sq_matrix(B, MATRIX_SIZE);
    zero_init_sq_matrix(C, MATRIX_SIZE);

    if (USE_TRANSPOSE) {
        B_T = malloc_sq_matrix(MATRIX_SIZE);
        if (B_T == NULL) {
            fprintf(stderr, "Allocation for transposed matrix failed. Exiting...");
        }
        transpose_sq_matrix(B, B_T, MATRIX_SIZE);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork(); 

        if (pid == -1) {
            perror("Failed creating child process!");
            exit(1); 
        }

        // Child process successfully created. Begin work...
        if (pid == 0) {

            // Based on the subdivisions per child process, calculate the starting and ending rows factoring in remainders.
            start_row = i * base_rows + (i < remainder ? i : remainder);
            end_row = start_row + base_rows + (i < remainder ? 1 : 0);
            
            // Because each process is working on it's own row of the C matrix, there is no contentions therefore locks are not needed.
            for (int r = start_row; r < end_row; r++) {
                if (USE_TRANSPOSE) mult_sq_matrices_row_transposed(r, A, B_T, C, MATRIX_SIZE);
                else mult_sq_matrices_row(r, A, B, C, MATRIX_SIZE);
            }
            exit(0); 
        }
    }

    // Wait for all child processes to terminate. If something goes wrong in termination, report errors.
    for (int i = 0; i < num_workers; i++) {
        int status; 
        pid_t child_pid = wait(&status);

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

    // If MATRIX_SIZE <= 1000, do single process matrix multiplication for verification otherwise use Frievalds algorithm.
    bool verified = verified_matrix(A, B, C, MATRIX_SIZE);   
    print_stats(MATRIX_SIZE, num_workers, verified, total_time_sec);

    free_sq_matrix_shared(A, MATRIX_SIZE);
    free_sq_matrix_shared(B, MATRIX_SIZE);
    free_sq_matrix_shared(C, MATRIX_SIZE);
    if (B_T) free_sq_matrix_shared(B_T, MATRIX_SIZE);
    return 0; 
}

// Handles cases where if the pipe is full, child process segmented row data gets processed in multiple read calls.
void safe_read(int fd, void *buf, size_t count) {
    size_t total_read = 0;
    while (total_read < count) {
        // If row data is segmented, loop until child with write access adds remaining row data into pipe.
        ssize_t n = read(fd, (char*)buf + total_read, count - total_read);
        if (n <= 0) break; 
        total_read += n;
    }
}

// Instead of using macros, I made it easier to just include arguments into the pipe program.
void arg_check(int argc, char *argv[]) {
     if (argc <= 3) {
        printf("Usage: %s [MATRIX_SIZE] [WORKERS] [USE_TRANSPOSE]\n", argv[0]);
        printf("---------------------------------------------------------------------------------\n");
        printf("MATRIX_SIZE - Set a matrix size                                     (2 - 10000)\n");
        printf("WORKERS - Set # of child processes. 0 for # of logical processors.  (0 - INT_MAX)\n");
        printf("USE_TRANSPOSE - Transposes posterior matrix to avoid cache misses.  (true or false)\n");
        exit(1); 
    }

    if (atoi(argv[1]) < 2 || atoi(argv[1]) > 10000) {
        printf("Incompatible MATRIX_SIZE! (2 - 10000)\n");
        exit(1); 
    }

    if (atoi(argv[2]) < 0) {
        printf("Incompatible WORKERS! (0 - INT_MAX)\n");
        exit(1); 
    }

    if (strcmp(argv[3], "true") != 0 && strcmp(argv[3], "false") != 0) {
        printf("Incompatible USE_TRANSPOSE! (true or false)\n");
        exit(1);
    }
}