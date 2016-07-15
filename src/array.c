#include "array.h"
#include "util.h"

rpg_array_t *
array_create(uint32_t n, size_t size) {
    rpg_array_t *a;
    
    ASSERT( n !=0 && size != 0);
    
    a = rpg_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }
    
    a->elts = rpg_calloc(n,  size);
    if (a->elts == NULL) {
        rpg_free(a);
        return NULL;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;

    return a;
}


void
array_destroy(rpg_array_t *a) {
    if (a->elts != NULL) {
        rpg_free(a->elts);
    }
    rpg_free(a);
}

void *
array_push(rpg_array_t *a) {
    size_t size;
    void *new, *elt;
        
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        new = rpg_realloc(a->elts, 2 * size);
        if (new == NULL) {
            return NULL;
        }
        a->elts = new;
        a->nalloc *= 2;
    }
    
    elt = (uint8_t *)a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}

void *
array_pop(rpg_array_t *a) {
    void *elt;
    
    if (a->nelts == 0) {
        return NULL;
    }

    a->nelts--;
    elt = (uint8_t *)a->elts + a->size * a->nelts;
    
    return elt;
}
