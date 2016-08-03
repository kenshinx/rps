#include "core.h"
#include "array.h"
#include "util.h"


#include <stdint.h>


rpg_status_t
array_init(rpg_array_t *a, uint32_t n, size_t size) {
    ASSERT(a !=  NULL);
    ASSERT( n !=0 && size != 0);

    a->elts = rpg_calloc(n,  size);
    if (a->elts == NULL) {
        rpg_free(a);
        return RPG_ENOMEM;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;

    return RPG_OK;
}

void
array_deinit(rpg_array_t *a) {
    if (a->elts != NULL) {
        rpg_free(a->elts);
    }
}


rpg_array_t *
array_create(uint32_t n, size_t size) {
    rpg_array_t *a;
    rpg_status_t status;
    
    a = rpg_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }

    status = array_init(a, n, size);
    if (status != RPG_OK) {
        return NULL;
    }
    

    return a;
}


void
array_destroy(rpg_array_t *a) {
    array_deinit(a);
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
    
    if (array_is_empty(a)) {
        return NULL;
    }

    a->nelts--;
    elt = (uint8_t *)a->elts + a->size * a->nelts;
    
    return elt;
}

void *
array_get(rpg_array_t *a, uint32_t idx) {
    void *elt;

    if (array_is_empty(a)) {
        return NULL;
    }
    
    elt = (uint8_t *)a->elts + a->size * idx;
    
    return elt;
}

void *
array_head(rpg_array_t *a) {

    if (array_is_empty(a)) {
        return NULL;
    }

    return array_get(a, a->nelts-1);
}

void
array_foreach(rpg_array_t *a, array_foreach_t func) {
    uint32_t i, nelts;
 
    for(i = 0, nelts = a->nelts; i < nelts;  i++) {
        void *elt = array_get(a, i);
        func(elt);
    }   
}
