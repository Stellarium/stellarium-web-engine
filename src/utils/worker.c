/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "worker.h"
#include "uthash.h"

#include <stdbool.h>
#include <stdlib.h>

enum {
    WORKER_RUNNING = 1,
    WORKER_FINISHED,
};

// How many threads in the pool
#define THREADS_COUNT 2

#ifdef HAVE_PTHREAD

#include <pthread.h>

typedef struct thread_t {
    pthread_t id;
    bool ready;
} thread_t;

static struct {
    thread_t threads[THREADS_COUNT];
    pthread_mutex_t rlock;
    pthread_cond_t global_cond;
    worker_t *waiting_worker;
    bool stop;
    bool initialized;
    worker_t *workers; // Hash map of workers.
} g = {
    .rlock = PTHREAD_MUTEX_INITIALIZER,
    .global_cond = PTHREAD_COND_INITIALIZER,
};

// The only part of the code that can run in different threads.
static void *thread_func(void *args)
{
    worker_t *w;
    thread_t *thread = (thread_t*)args;
    int r;

    thread->ready = true;
    while (true) {
        pthread_mutex_lock(&g.rlock);
        while (g.waiting_worker == NULL && !g.stop)
            pthread_cond_wait(&g.global_cond, &g.rlock);
        if (g.stop) {
            pthread_mutex_unlock(&g.rlock);
            break;
        }
        thread->ready = false;
        w = g.waiting_worker;
        g.waiting_worker = NULL;
        pthread_mutex_unlock(&g.rlock);

        r = w->fn(w);

        pthread_mutex_lock(&g.rlock);
        w->ret = r;
        w->state = WORKER_FINISHED;
        thread->ready = true;
        pthread_mutex_unlock(&g.rlock);
    }
    return NULL;
}

static void g_init(void)
{
    int i;
    for (i = 0; i < THREADS_COUNT; i++)
        pthread_create(&g.threads[i].id, NULL, thread_func, &g.threads[i]);
    g.initialized = true;
}

void worker_init(worker_t *w, int (*fn)(worker_t *w))
{
    if (!g.initialized) g_init();
    w->state = 0;
    w->ret = 0;
    w->fn = fn;
}

static bool is_thread_ready(void)
{
    int i;
    if (g.waiting_worker != NULL) return false;
    for (i = 0; i < THREADS_COUNT; i++) {
        if (g.threads[i].ready) {
            return true;
        }
    }
    return false;
}

int worker_iter(worker_t *w)
{
    pthread_mutex_lock(&g.rlock);
    if (w->state == 0) {
        if (is_thread_ready()) {
            w->state = WORKER_RUNNING;
            g.waiting_worker = w;
            pthread_cond_signal(&g.global_cond);
        }
    }
    pthread_mutex_unlock(&g.rlock);
    return w->state == WORKER_FINISHED;
}

bool worker_is_running(worker_t *w)
{
    bool ret;
    pthread_mutex_lock(&g.rlock);
    ret = w->state == WORKER_RUNNING;
    pthread_mutex_unlock(&g.rlock);
    return ret;
}

#else // No pthread, basic non threaded implementations.

void worker_init(worker_t *w, int (*fn)(worker_t *w))
{
    memset(w, 0, sizeof(*w));
    w->fn = fn;
}

int worker_iter(worker_t *w)
{
    if (w->state) return 1;
    w->fn(w);
    w->state = 1;
    return 1;
}

bool worker_is_running(worker_t *w)
{
    return false;
}

#endif
