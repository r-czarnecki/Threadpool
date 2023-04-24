#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>

#include "threadpool.h"

typedef unsigned long long int LLU;

typedef struct data {
    sem_t *sem;
    sem_t *done;
    LLU fields, *visited;
    LLU *sum;
    LLU v, t;
} data_t;

void msleep(LLU ms) {
    struct timespec time;
    time.tv_sec = ms / 1000;
    time.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&time, &time);
}

void function(void *arg, size_t size __attribute__((unused))) {
    data_t *data = (data_t*)(arg);

    msleep(data->t);

    sem_wait(data->sem);
    *data->sum += data->v;
    *data->visited += 1;
    if(*data->visited == data->fields)
        sem_post(data->done);
    sem_post(data->sem);
    free(data);
}

int main() {
    thread_pool_t pool;
    thread_pool_init(&pool, 4);
    sem_t sem;
    sem_t done;
    sem_init(&sem, 0, 1);
    sem_init(&done, 0, 0);
    LLU n, k, visited, *sum;
    visited = 0;

    scanf("%llu%llu", &k, &n);
    sum = malloc(k * sizeof(LLU));
    if(sum == 0) {
        fprintf(stderr, "memory allocation failed\n");
        exit(1);
    }

    for(LLU i = 0; i < k; i++) {    
        for(LLU j = 0; j < n; j++) {
            if(j == 0)
                sum[i] = 0;
            LLU v, t;
            scanf("%llu%llu", &v, &t);
            data_t *d = malloc(sizeof(data_t));
            *d = (data_t){.sem = &sem, .done = &done, .sum = &sum[i], .fields = n * k, .visited = &visited, .v = v, .t = t};
            runnable_t runnable = (runnable_t){.function = function, .arg = d, .argsz = sizeof(data_t)};
            if(defer(&pool, runnable) != 0) {
                fprintf(stderr, "defer failed");
                exit(1);
            }
        }
    }

    sem_wait(&done);
    for(LLU i = 0; i < k; i++)
        printf("%llu\n", sum[i]);
    sem_destroy(&done);
    thread_pool_destroy(&pool);
    free(sum);
}