#include "core.h"
#include "queue.h"


int
queue_init(rps_queue_t *q, uint32_t n) {
    ASSERT(q != NULL);
    ASSERT(n != 0);

    q->head = 0;
    q->tail = n - 1;
    q->nelts = n;
    q->nalloc = 0;

    q->elts = rps_alloc(n * sizeof(void *));
    if (q->elts == NULL) {
        return RPS_ENOMEM;
    }

    return RPS_OK;
}

void
queue_deinit(rps_queue_t *q) {
    if (q->elts != NULL) {
        rps_free(q->elts);
    }
    q->elts = NULL;
}

int
queue_en(rps_queue_t *q, void *e) {   
    if (queue_is_full(q)) {
        log_warn("queue overflow.");
        return RPS_EQUEUE;
    }

    q->tail = (q->tail + 1) % q->nelts;
    q->elts[q->tail] = e;
    q->nalloc += 1;

    return RPS_OK;
}

void *
queue_de(rps_queue_t *q) {
    void *e;

    if (queue_is_empty(q)) {
        return NULL;
    }

    e = q->elts[q->head];
    q->head = (q->head + 1) % q->nelts;
    q->nalloc -= 1;
    
    return e;
}

void 
queue_iter(rps_queue_t *q, queue_iter_t iter) {
    uint32_t i;

    i = q->head;

    while(i != q->tail) {
        iter(q->elts[i]);
        i = (i + 1) % q->nelts;
    }

    iter(q->elts[i]);
}

void
queue_print(void *e) {
    long int x;
    
    x = (long int)e;

    printf("%ld\n", x);
}

/*
int main() {
    rps_queue_t q;
    unsigned long x, y;

    x = 2;
    y = 6;

    queue_init(&q, 10);
    queue_en(&q, (void *)x);
    queue_en(&q, (void *)y);

    queue_iter(&q, queue_print);
}
*/
