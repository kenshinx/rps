#ifndef _RPS_HASHTABLE_H
#define _RPS_HASHTABLE_H

#include <stddef.h>
#include <stdint.h>

typedef void (*rps_hashfunc_t)(const void *key, int len, uint32_t seed, void *out);

struct hash_entry {
    void                *key;
    size_t              key_size;

    void                *value;
    size_t              value_size;
    
    struct hash_entry   *next;
    
};

struct rps_hashtable_s {

    
    struct hash_entry   **buckets;
    /* bucket size */
    uint32_t            size;

    uint32_t            seed;

    rps_hashfunc_t      hashfunc;
};

typedef struct rps_hashtable_s rps_hashtable_t;

int hashtable_init(rps_hashtable_t *ht, uint32_t nbuckets);
rps_hashtable_t *hashtable_create(uint32_t nbuckets);




#endif
