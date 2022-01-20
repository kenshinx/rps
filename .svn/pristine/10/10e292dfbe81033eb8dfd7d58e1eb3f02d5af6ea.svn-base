#ifndef _RPS_HASHMAP_H
#define _RPS_HASHMAP_H

#include <stddef.h>
#include <stdint.h>

#define HASHMAP_DEFAULT_COLLISIONS  0.05

#define hashmap_n(_m)                   \
    ((_m)->count)

#define hashmap_is_empty(_m)            \
    ((_m)->count == 0)

typedef void (*hashmap_hash_t)(const void *key, int len, uint32_t seed, void *out);
typedef void (*hashmap_foreach_t) (void *key, size_t key_size, void *value, size_t value_size);
typedef void (*hashmap_foreach2_t) (void *data);

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

    hashmap_hash_t      hashfunc;
};

typedef struct rps_hashmap_s rps_hashmap_t;

struct rps_hashmap_iterator_s {
    struct rps_hashmap_s    *map;
    uint32_t                index;
    uint32_t                listele;
};

typedef struct rps_hashmap_iterator_s rps_hashmap_iterator_t;

int hashmap_init(rps_hashmap_t *map, uint32_t nbuckets, double max_load_factor);
void hashmap_deinit(rps_hashmap_t *map);
rps_hashmap_t *hashmap_create(uint32_t nbuckets, double max_load_factor);

void hashmap_rehash(rps_hashmap_t *map, uint32_t new_size);

void * hashmap_get(rps_hashmap_t *map, void *key, size_t key_size, 
        size_t *value_size);
void hashmap_set(rps_hashmap_t *map, void *key, size_t key_size, 
        void *value, size_t value_size);
struct hashmap_entry * hashmap_get_random_entry(rps_hashmap_t *map);

int hashmap_has(rps_hashmap_t *map, void *key, size_t key_size);
int hashmap_remove(rps_hashmap_t *map, void *key, size_t key_size);

void hashmap_foreach(rps_hashmap_t *map, hashmap_foreach_t func);
void hashmap_foreach2(rps_hashmap_t *map, hashmap_foreach2_t func);
void hashmap_deepcopy(rps_hashmap_t *dst, rps_hashmap_t *src);

void hashmap_iterator_init(rps_hashmap_iterator_t *iter, rps_hashmap_t *map);
void hashmap_iterator_deinit(rps_hashmap_iterator_t *iter);
struct hashmap_entry *hashmap_next(rps_hashmap_iterator_t *iter);

#endif
