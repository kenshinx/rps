#ifndef _RPG_ARRAY_H
#define _RPG_ARRAY_H

#include "core.h"

#include <stdio.h>
#include <stdint.h>

typedef void (*array_foreach_t)(void *);

struct rpg_array {
    void            *elts;
    uint32_t        nelts;
    size_t          size;
    uint32_t        nalloc;
};

typedef struct rpg_array rpg_array_t;

static inline uint32_t
array_n(rpg_array_t *a) {
    return a->nelts;
}

#define array_is_empty(_a)                             \
    ((_a->nelts) == 0)          

#define array_is_full(_a)                              \
    ((_a)->nelts == (_a)->nalloc)

rpg_status_t array_init(rpg_array_t *a, uint32_t n, size_t size);
rpg_array_t *array_create(uint32_t n, size_t size);
void array_destroy(rpg_array_t *a);
void *array_push(rpg_array_t *a);
void *array_pop(rpg_array_t *a);
void *array_get(rpg_array_t *a, uint32_t idx);
void *array_head(rpg_array_t *a);
void array_foreach(rpg_array_t *a, array_foreach_t func);


#endif
