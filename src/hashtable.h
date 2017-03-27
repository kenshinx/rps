#ifndef _RPS_HASHTABLE_H
#define _RPS_HASHTABLE_H

#include "_string.h"
#include "util.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef (*rps_hashfunc_t)(const void *key, int len, uint32_t seed, void *out);

struct hash_entry {
    rps_str_t           *key;

    void                *value;
    /* size of value */
    size_t              size;
    
    struct hash_entry   *next;
    
};

struct rps_hashtable_s {

    
    struct hash_entry   **buckets;
    /* bucket size */
    uint32_t            size;

    uint32_t            seed;

    rps_hashfunc_t      *hashfunc;
};

typedef struct rps_hashtable_s rps_hashtable_t;

rps_hashtable_t *hashtable_create(uint32_t nbuckets);



#endif
