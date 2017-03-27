#ifndef _RPS_HASHMAP_H
#define _RPS_HASHMAP_H

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

struct rps_hashmap_s {

    
    struct hash_entry   **buckets;

    /* allocate keys */
    uint32_t            count;

    /* bucket size */
    uint32_t            size;

    uint32_t            seed;

    rps_hashfunc_t      hashfunc;
};

typedef struct rps_hashmap_s rps_hashmap_t;

int hashmap_init(rps_hashmap_t *map, uint32_t nbuckets);
rps_hashmap_t *hashmap_create(uint32_t nbuckets);
void hashmap_destroy(rps_hashmap_t *map);
void hashmap_set(rps_hashmap_t *map, void *key, size_t key_size, 
        void *value, size_t value_size);




#endif
