#include "core.h"
#include "array.h"
#include "util.h"


#include <stdint.h>

int
array_init(rps_array_t *a, uint32_t n, size_t size) {
    ASSERT(a !=  NULL);
    ASSERT( n !=0 && size != 0);

    a->elts = rps_calloc(n,  size);
    if (a->elts == NULL) {
        rps_free(a);
        return RPS_ENOMEM;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;

    return RPS_OK;
}

void
array_deinit(rps_array_t *a) {
    if (a->elts != NULL) {
        rps_free(a->elts);
    }
	array_null(a);
}


rps_array_t *
array_create(uint32_t n, size_t size) {
    rps_array_t *a;
    rps_status_t status;
    
    a = rps_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }

    status = array_init(a, n, size);
    if (status != RPS_OK) {
        return NULL;
    }
    

    return a;
}


void
array_destroy(rps_array_t *a) {
    array_deinit(a);
    rps_free(a);
}

void *
array_push(rps_array_t *a) {
    size_t size;
    void *new, *elt;
        
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        new = rps_realloc(a->elts, 2 * size);
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
array_pop(rps_array_t *a) {
    void *elt;
    
    if (array_is_empty(a)) {
        return NULL;
    }

    a->nelts--;
    elt = (uint8_t *)a->elts + a->size * a->nelts;
    
    return elt;
}

void *
array_get(rps_array_t *a, uint32_t idx) {
    void *elt;

    if (array_is_empty(a)) {
        return NULL;
    }
    
    elt = (uint8_t *)a->elts + a->size * idx;
    
    return elt;
}

void *
array_head(rps_array_t *a) {

    if (array_is_empty(a)) {
        return NULL;
    }

    return array_get(a, a->nelts-1);
}

void *
array_random(rps_array_t *a) {
    int i;

    if (array_is_empty(a)) {
        return NULL;
    }

    i = rps_random(array_n(a));

    return array_get(a, i);
}

void
array_foreach(rps_array_t *a, array_foreach_t func) {
    uint32_t i, nelts;
 
    for(i = 0, nelts = a->nelts; i < nelts;  i++) {
        void *elt = array_get(a, i);
        func(elt);
    }   
}

void 
array_swap(rps_array_t **a, rps_array_t **b) {
    rps_array_t *tmp;
    
    tmp = *a;
    *a = *b;
    *b = tmp;
}
