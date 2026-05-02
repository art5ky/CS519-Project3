/*  CS519, Spring 2026: Project 1 - Part 2
    Written by: Arthur Levitsky
*/

#ifndef LOCKS
#define LOCKS

#include <stdatomic.h>

typedef struct {
    atomic_uint ticket; 
    char ticket_padding[64];
    atomic_uint now_serving;
    char serving_padding[64];
} ticket_lock_t; 

ticket_lock_t* tl_create(); 
void tl_destroy(ticket_lock_t* lock);
void tl_acquire(ticket_lock_t* lock);
void tl_release(ticket_lock_t* lock);

int semaphore_create(int num_sems);
void semaphore_destroy(int sem_id);
void semaphore_init(int sem_id, int sem_num, int init_valve);
void semaphore_release(int sem_id, int sem_num);
void semaphore_reserve(int sem_id, int sem_num);

#endif