#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "threadpool.h"

static int *destroy_all() {
    static int destroy_all = 0;
    return &destroy_all;
} 

static struct sigaction action;
static sigset_t block_mask;

static void catch(int sig __attribute__((unused))) {
    *destroy_all() = 1;
}

inline static void syserr(const char *msg)  
{
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

inline static void check(int err, const char *msg) {
    if(err != 0)
        syserr(msg);
}

static int push_back(runnableList *list, runnable_t *runnable) {
    runnableListElement_t *new = malloc(sizeof(runnableListElement_t));
    if(new == NULL)
        return 1;
    new->runnable = *runnable;
    new->next = NULL;

    if(*list == NULL) {
        new->prev = NULL;
        new->last = new;
        new->size = 1;
        *list = new;
    }
    else {
        new->prev = (*list)->last;
        (*list)->size++;
        (*list)->last->next = new;
        (*list)->last = new;
    }

    return 0;
}

static void pop_front(runnableList *list) {
    runnableListElement_t *tmp = (*list)->next;
    if(tmp != NULL) {
        tmp->last = (*list)->last;
        tmp->prev = NULL;
        tmp->size = (*list)->size - 1;
    }
    free(*list);
    *list = tmp;
}

static void clearList(runnableList *list) {
    while(*list != NULL) 
        pop_front(list);
}

static int size(runnableList list) {
    if(list == NULL)
        return 0;
    return list->size;
}


static void *worker(void *data) {
    thread_pool_t *pool = (thread_pool_t*)(data);
    runnable_t runnable;

    while(1) {
        check(pthread_mutex_lock(&pool->guard), "guard lock");

        if(*destroy_all() == 1 && size(pool->list) == 0) {
            thread_pool_destroy(pool);
            check(pthread_mutex_unlock(&pool->guard), "guard unlock");
            return 0;
        }

        while(size(pool->list) == 0 && pool->destroyed == 0)
            check(pthread_cond_wait(&pool->wait, &pool->guard), "cond wait");

        if(pool->destroyed == 1) {
            check(pthread_mutex_unlock(&pool->guard), "guard unlock");
            return 0;
        }

        runnable = (pool->list)->runnable;
        pop_front(&pool->list);

        check(pthread_mutex_unlock(&pool->guard), "guard unlock");

        (*runnable.function)(runnable.arg, runnable.argsz);
    }
}

int thread_pool_init(thread_pool_t *pool, size_t num_threads) { 
    pthread_attr_t attr;
    sigemptyset(&block_mask);
    action.sa_handler = catch;
    action.sa_mask = block_mask;
    action.sa_flags = 0;
    if(sigaction(SIGINT, &action, NULL) == -1)
        syserr("sigaction");

    pool->list = NULL;
    pool->destroyed = 0;
    pool->pool_size = num_threads;
    check(pthread_attr_init(&attr), "pthread_attr init");
    check(pthread_mutex_init(&pool->guard, 0), "mutex init");
    check(pthread_cond_init(&pool->wait, 0), "cond init");
    
    pool->workers = malloc(num_threads * sizeof(pthread_t));
    if(pool->workers == NULL)
        syserr("pthread malloc");
    
    for(size_t i = 0; i < num_threads; i++)
        check(pthread_create(&pool->workers[i], &attr, worker, pool), "pthread init");
    return 0; 
}

void thread_pool_destroy(struct thread_pool *pool) {
    check(pthread_mutex_lock(&pool->guard), "guard lock");
    pool->destroyed = 1;
    check(pthread_cond_broadcast(&pool->wait), "cond broadcast");
    check(pthread_mutex_unlock(&pool->guard), "guard unlock");

    for(size_t i = 0; i < pool->pool_size; i++)
        check(pthread_join(pool->workers[i], NULL), "pthread join");
    
    clearList(&pool->list);
    check(pthread_mutex_destroy(&pool->guard), "guard destroy");
    check(pthread_cond_destroy(&pool->wait), "cond destroy");
    free(pool->workers);
}

int defer(struct thread_pool *pool, runnable_t runnable) {
    check(pthread_mutex_lock(&pool->guard), "guard lock");
    if(*destroy_all() == 1 || pool->destroyed == 1 || push_back(&pool->list, &runnable) != 0) {
        check(pthread_mutex_unlock(&pool->guard), "guard unlock");  
        return 1;
    }
    check(pthread_cond_signal(&pool->wait), "cond signal");
    check(pthread_mutex_unlock(&pool->guard), "guard unlock");    
    return 0; 
}

