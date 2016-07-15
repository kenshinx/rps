#ifndef _RPG_ARRAY_H
#define _RPG_ARRAY_H


#include <stdio.h>
#include <stdint.h>

typedef struct rpg_array {
    void            *elts;
    uint32_t        nelts;
    size_t          size;
    uint32_t        nalloc;
} rpg_array_t;

static inline uint32_t
array_n(rpg_array_t *a) {
    return a->nelts;
}

rpg_array_t *array_create(uint32_t n, size_t size);
void *array_destory(rpg_array_t *a);
void *array_push(rpg_array_t *a);
void *array_pop(rpg_array_t *a);


#endif
