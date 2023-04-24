#include <stdio.h>
#include <stdlib.h>

#include "future.h"

typedef unsigned long long int LLU;

typedef struct arg {
    LLU result;
    LLU n;
} arg_t;

void *factorial(void *arg, size_t argsz __attribute__((unused)), size_t *ressz __attribute__((unused))) {
    arg_t *ar = (arg_t*)(arg);
    ar->result *= ar->n;
    ar->n++;
    return ar;
}

int main() {
    thread_pool_t pool;
    thread_pool_init(&pool, 3);
    LLU n;
    scanf("%llu", &n);

    future_t *futures = malloc(n * sizeof(future_t));
    if(futures == NULL) {
        fprintf(stderr, "memory allocation failed");
        exit(1);
    }

    arg_t arg = (arg_t){.result = 1, .n = 1};
    callable_t callable = (callable_t){.function = factorial, .arg = &arg, .argsz = sizeof(arg_t)};

    for(LLU i = 1; i <= n; i++) {
        if(i == 1)
            async(&pool, &futures[i-1], callable);
        else
            map(&pool, &futures[i-1], &futures[i-2], factorial);
    }

    arg = *(arg_t*)await(&futures[n-1]);
    printf("%llu\n", arg.result);

    for(LLU i = 0; i < n; i++)
        future_destroy(&futures[i]);

    free(futures);
    thread_pool_destroy(&pool);
}
