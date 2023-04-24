#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>
#include <pthread.h>


typedef struct runnable {
    void (*function)(void *, size_t);
    void *arg;
    size_t argsz;
} runnable_t;

typedef struct runnableListElement {
    runnable_t runnable;
    struct runnableListElement *next;
    struct runnableListElement *prev;
    struct runnableListElement *last;
    int size;
} runnableListElement_t;

typedef runnableListElement_t* runnableList;

typedef struct thread_pool {
    runnableList list;
    pthread_mutex_t guard;
    pthread_cond_t wait;
    pthread_t *workers;
    size_t pool_size;
    int destroyed;
} thread_pool_t;

int thread_pool_init(thread_pool_t *pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t *pool);

int defer(thread_pool_t *pool, runnable_t runnable);

#endif
