#ifndef _RPS_HASHMAP_H
#define _RPS_HASHMAP_H

#include <stddef.h>
#include <stdint.h>


typedef void (*rps_hashfunc_t)(const void *key, int len, uint32_t seed, void *out);

struct hashmap_entry {
    void                *key;
    size_t              key_size;

    void                *value;
    size_t              value_size;
    
    struct hashmap_entry   *next;
    
};

struct rps_hashmap_s {

    
    struct hashmap_entry   **buckets;

    /* allocate keys */
    uint32_t            count;

    /* bucket size */
    uint32_t            size;

    uint32_t            seed;

    uint32_t            collisions;
    /* autoresize will be triggered if max load factor reached */ 
    double              max_load_factor;

    rps_hashfunc_t      hashfunc;
};

typedef struct rps_hashmap_s rps_hashmap_t;

int hashmap_init(rps_hashmap_t *map, uint32_t nbuckets, double max_load_factor);
void hashmap_deinit(rps_hashmap_t *map);
rps_hashmap_t *hashmap_create(uint32_t nbuckets, double max_load_factor);

void hashmap_rehash(rps_hashmap_t *map, uint32_t new_size);

void * hashmap_get(rps_hashmap_t *map, void *key, size_t key_size, 
        size_t *value_size);
void hashmap_set(rps_hashmap_t *map, void *key, size_t key_size, 
        void *value, size_t value_size);

int hashmap_has(rps_hashmap_t *map, void *key, size_t key_size);
int hashmap_remove(rps_hashmap_t *map, void *key, size_t key_size);
uint32_t hashmap_n(rps_hashmap_t *map);



#endif
