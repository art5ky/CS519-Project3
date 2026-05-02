/*  CS519, Spring 2026: Project 1 - Part 2
    Written by: Arthur Levitsky
    Benchmarking tools which includes checking matrix correctness.
*/

#include <sys/time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../headers/sqmatrix.h"

void print_stats(int matrix_size, int num_processes, bool verified, double elapsed) {
        printf("Input size: %d x %d\n",        matrix_size, matrix_size);
        printf("Number of processes: %d\n",    num_processes);
        printf("Verification: %s\n",           verified ? "PASS" : "FAIL");
        printf("Total runtime: %.6f seconds\n", elapsed);
    }

double get_total_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Check to see if A and B have the same entires and are the same matrix or not. 
bool same_matrix(int **A, int **B, size_t size) {
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < size; j++) {
            if (A[i][j] != B[i][j]) return false;
        }
    }
    return true; 
}

// Using Frievalds algorithm for checking matrix multiplication for MATRIX_SIZE > 2000.
bool frievalds_verify(int **A, int **B, int **C, size_t size) {
    int *r = (int *)malloc(size * sizeof(int));
    int *B_r = (int *)malloc(size * sizeof(int));
    int *C_r = (int *)malloc(size * sizeof(int));
    int *AB_r = (int *)malloc(size * sizeof(int));

    for (size_t i = 0; i < size; i++) {
        r[i] = rand() % 2;
    }

    mult_sq_matrix_vec(B, r, B_r, size);
    mult_sq_matrix_vec(C, r, C_r, size);
    mult_sq_matrix_vec(A, B_r, AB_r, size);

    for (size_t i = 0; i < size; i++) {
        if (AB_r[i] != C_r[i]) {

            free(r);
            free(B_r);
            free(C_r);
            free(AB_r);
            return false; 
        }
    }
    
    free(r);
    free(B_r);
    free(C_r);
    free(AB_r);
    return true; 
}

bool verified_matrix(int **A, int **B, int **C, size_t size) {
    if (size <= 1000) {
        int **prod_matrix = malloc_sq_matrix(size);
        zero_init_sq_matrix(prod_matrix, size);

        if (prod_matrix == NULL) {
            fprintf(stderr, "Allocation for matrix D failed!");
            return false; 
        }
        mult_sq_matrices_full(A, B, prod_matrix, size);
        bool verified = same_matrix(C, prod_matrix, size);
        free_sq_matrix(prod_matrix);
        return verified;
    } else {
        return frievalds_verify(A, B, C, size);
    }
}