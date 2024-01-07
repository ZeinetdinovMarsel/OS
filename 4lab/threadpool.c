#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <assert.h>

#define QUEUE_SIZE 20
#define NUMBER_OF_THREADS 8

typedef struct {
    void (*function)(void *p);
    void *data;
} task;

task work_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = -1;
int queue_count = 0;

pthread_t worker_threads[NUMBER_OF_THREADS];
pthread_mutex_t queue_mutex;
sem_t task_semaphore;

void Pthread_mutex_lock(pthread_mutex_t *mutex) {
    int rc = pthread_mutex_lock(mutex);
    assert(rc == 0);
}

void Pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int rc = pthread_mutex_unlock(mutex);
    assert(rc == 0);
}

void Pthread_mutex_init(pthread_mutex_t *mutex) {
    int rc = pthread_mutex_init(mutex, NULL);
    assert(rc == 0);
}

void enqueue(task t) {
    Pthread_mutex_lock(&queue_mutex);
    if (queue_count < QUEUE_SIZE) {
        queue_rear = (++queue_rear) % QUEUE_SIZE;
        work_queue[queue_rear] = t;
        queue_count++;
        sem_post(&task_semaphore);
    }
    Pthread_mutex_unlock(&queue_mutex);
}

task dequeue() {
    task worktodo;
    sem_wait(&task_semaphore);
    Pthread_mutex_lock(&queue_mutex);
    worktodo = work_queue[queue_front];
    queue_front = (++queue_front) % QUEUE_SIZE;
    queue_count--;
    Pthread_mutex_unlock(&queue_mutex);
    return worktodo;
}

void execute(void (*somefunction)(void *p), void *p) {
    (*somefunction)(p);
}

void *worker(void *param) {
    while (1) {
        task worktodo = dequeue();
        if (worktodo.function == NULL)
            pthread_exit(NULL);
        execute(worktodo.function, worktodo.data);
    }
}


int pool_submit(void (*somefunction)(void *p), void *p) {
    task new_task;
    new_task.function = somefunction;
    new_task.data = p;
    enqueue(new_task);
    return 0;
}

void pool_init(void) {
    Pthread_mutex_init(&queue_mutex);
    sem_init(&task_semaphore, 0, 0);
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        pthread_create(&worker_threads[i], NULL, worker, NULL);
    }
}

void pool_shutdown(void) {
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        pool_submit(NULL, NULL);
    }
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    pthread_mutex_destroy(&queue_mutex);
    sem_destroy(&task_semaphore);
}