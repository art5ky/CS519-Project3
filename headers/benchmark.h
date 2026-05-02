/*  CS519, Spring 2026: Project 1 - Part 2
    Written by: Arthur Levitsky
*/

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <sys/time.h>
#include <stdbool.h>
#include "../headers/sqmatrix.h"

void print_stats(int matrix_size, int num_processes, int verified, double elapsed);
double get_total_time(struct timespec start, struct timespec end);

bool same_matrix(int **A, int **B);
bool frievalds_verify(int **A, int **B, int **C, size_t size);
bool verified_matrix(int **A, int **B, int **C, size_t size);

#endif