/*
 * Circular array based FIFO queue
 */


#ifndef _RPS_QUEUE_H
#define _RPS_QUEUE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

typedef void (*queue_iter_t) (void *);

struct rps_queue_s {
    void        **elts;
    uint32_t    head;
    uint32_t    tail;
    uint32_t    nelts;
    uint32_t    nalloc;
};

typedef struct rps_queue_s rps_queue_t;

static inline void
queue_null(rps_queue_t *q) {
    q->elts = NULL;
    q->head = 0;
    q->tail = 0;
    q->nelts = 0;
    q->nalloc = 0;
}

#define queue_n(_q)                                     \
    ((_q)->nalloc)

#define queue_is_empty(_q)                              \
    ((_q)->nalloc <= 0)

#define queue_is_null(_q)                               \
    (((_q)->nalloc <= 0) && ((_q)->elts == NULL))

#define queue_is_full(_q)                               \
    ((_q)->nalloc >= (_q)->nelts)

int queue_init(rps_queue_t *q, uint32_t n);
void queue_deinit(rps_queue_t *q);
int queue_en(rps_queue_t *q, void *e);
void *queue_de(rps_queue_t *q);
void queue_iter(rps_queue_t *q, queue_iter_t iter);



#endif
