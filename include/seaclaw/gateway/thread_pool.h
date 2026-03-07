#ifndef SC_THREAD_POOL_H
#define SC_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*sc_work_fn)(void *arg);

typedef struct sc_thread_pool sc_thread_pool_t;

/* Create a pool with `n` worker threads */
sc_thread_pool_t *sc_thread_pool_create(size_t n);

/* Submit work. Returns false if the queue is full or pool is shutting down. */
bool sc_thread_pool_submit(sc_thread_pool_t *pool, sc_work_fn fn, void *arg);

/* Drain pending work and join all threads. */
void sc_thread_pool_destroy(sc_thread_pool_t *pool);

#endif /* SC_THREAD_POOL_H */
