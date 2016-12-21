#ifndef _RPS_ARRAY_H
#define _RPS_ARRAY_H

#include "core.h"

#include <stdio.h>
#include <stdint.h>

typedef void (*array_foreach_t)(void *);

struct rps_array {
    void            *elts;
    uint32_t        nelts;
    size_t          size;
    uint32_t        nalloc;
};

typedef struct rps_array rps_array_t;

static inline uint32_t
array_n(rps_array_t *a) {
    return a->nelts;
}

#define array_is_empty(_a)                             \
    ((_a->nelts) == 0)          

#define array_is_full(_a)                              \
    ((_a)->nelts == (_a)->nalloc)

rps_status_t array_init(rps_array_t *a, uint32_t n, size_t size);
rps_array_t *array_create(uint32_t n, size_t size);
void array_deinit(rps_array_t *a);
void array_destroy(rps_array_t *a);
void *array_push(rps_array_t *a);
void *array_pop(rps_array_t *a);
void *array_get(rps_array_t *a, uint32_t idx);
void *array_head(rps_array_t *a);
void array_foreach(rps_array_t *a, array_foreach_t func);


#endif
