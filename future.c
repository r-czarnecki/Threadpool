#include <stdio.h>
#include <stdlib.h>

#include "future.h"

typedef void *(*function_t)(void *);

typedef struct mapped_future {
    future_t *from;
    void *(*function)(void *, size_t, size_t *);
} mapped_future_t;

inline static void syserr(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

inline static void check(int err, const char *msg) {
    if(err != 0)
        syserr(msg);
}

static void fun(void *data, size_t size __attribute__((unused))) {
    future_t *future = (future_t*)(data);
    callable_t *callable = &future->callable;
    future->result = (*callable->function)(callable->arg, callable->argsz, &future->result_size);
    
    check(pthread_mutex_lock(&future->guard), "guard lock");
    future->isReady = 1;
    check(pthread_cond_broadcast(&future->wait), "cond broadcast");
    check(pthread_mutex_unlock(&future->guard), "guard unlock");
}

static void *map_fun(void *data, size_t size __attribute__((unused)), size_t *result_size) {
    mapped_future_t *mapped = (mapped_future_t*)(data);
    void *result = await(mapped->from);
    result = (*mapped->function)(result, mapped->from->result_size, result_size);
    free(mapped);
    return result;
}

static void future_init(future_t *future) {
    check(pthread_mutex_init(&future->guard, 0), "guard init");
    check(pthread_cond_init(&future->wait, 0), "cond init");
    future->result = NULL;
    future->isReady = 0;
}

int async(thread_pool_t *pool, future_t *future, callable_t callable) {
    future_init(future);
    future->callable = callable;
    runnable_t runnable = (runnable_t){.function = fun, .arg = future, .argsz = sizeof(future_t)};
    if(defer(pool, runnable) != 0)
        return 1;
    return 0;
}

int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *)) {

    mapped_future_t *mapped = malloc(sizeof(mapped_future_t));
    if(mapped == NULL) {
        fprintf(stderr, "memory allocation failed");
        exit(1);
    }

    *mapped = (mapped_future_t){.from = from, .function = function};
    callable_t callable = (callable_t){.function = map_fun, .arg = mapped, .argsz = sizeof(mapped_future_t)};
    async(pool, future, callable);
    return 0;
}

void *await(future_t *future) {
    check(pthread_mutex_lock(&future->guard), "guard lock");
    while(future->isReady == 0)
        check(pthread_cond_wait(&future->wait, &future->guard), "cond wait");
    check(pthread_mutex_unlock(&future->guard), "guard unlock");
    return future->result; 
}

int future_destroy(future_t *future) {
    check(pthread_mutex_destroy(&future->guard), "guard destroy");
    check(pthread_cond_destroy(&future->wait), "cond destroy");

    return 0;
}