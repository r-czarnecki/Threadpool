#ifndef FUTURE_H
#define FUTURE_H

#include "threadpool.h"

typedef struct callable {
    void *(*function)(void *, size_t, size_t *);
    void *arg;
    size_t argsz;
} callable_t;

typedef struct future {
    pthread_mutex_t guard;
    pthread_cond_t wait;
    thread_pool_t *pool;
    callable_t callable;
    size_t result_size;
    void *result;
    int isReady;
} future_t;

int async(thread_pool_t *pool, future_t *future, callable_t callable);

int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *));

void *await(future_t *future);

int future_destroy(future_t *future);

#endif
