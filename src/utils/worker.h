/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: worker.h
 * Some basic threading functions.
 *
 * A worker is simply a task that run in a thread pool.  We can create a worker
 * with <worker_init> and then run it by calling <worker_iter> as many times
 * as we want, until it returns a non zero value.
 */

#ifndef WORKER_H
#define WORKER_H

#include <stdbool.h>

typedef struct worker worker_t;

struct worker
{
    int (*fn)(worker_t *w);
    void *user;
    int ret;
    int state;
};

/*
 * Function: worker_init
 * Initialize the worker struct to run a given function in a thread.
 */
void worker_init(worker_t *w, int (*fn)(worker_t *w));

/*
 * Function: worker_iter
 * Execute the worker function.
 *
 * This will attempt to start the worker function in a separate thread.
 * If the function is already running or if no thread is ready, it does
 * nothing.
 *
 * We can call this in a loop until it returns a non zero value to make it
 * work like a simple future object.
 *
 * Return:
 *   0 if the function has not finished yet.
 */
int worker_iter(worker_t *worker);

/*
 * Function: worker_is_running
 * Return whether a worker is currently running.
 */
bool worker_is_running(worker_t *worker);

#endif // WORKER_H
