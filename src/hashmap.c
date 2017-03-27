#include "hashmap.h"
#include "murmur3.h"
#include "core.h"

#include "stdlib.h"

//max int32 
#define MAX_SEED  2147483647 

static struct hash_entry *
hashmap_entry_create(void *key, size_t key_size, void *value, size_t value_size) {
    struct hash_entry *entry;

    ASSERT(key_size > 0);
    ASSERT(value_size > 0);

    entry = rps_alloc(sizeof(struct hash_entry));
    if (entry == NULL) {
        return NULL;
    }
    
    entry->key = rps_alloc(key_size);
    if (entry->key == NULL) {
        rps_free(entry);
        return NULL;
    }
    entry->key_size = key_size;
    memcpy(entry->key, key, key_size);

    entry->value = rps_alloc(value_size);
    if (entry->value == NULL) {
        rps_free(entry->key);
        rps_free(entry);
        return NULL;
    }
    entry->value_size = value_size;
    memcpy(entry->value, value, value_size);

    entry->next = NULL;

    return entry;
}

static void
hashmap_entry_deinit(struct hash_entry *entry) {
    if (entry->key != NULL) {
        rps_free(entry->key);
        entry->key = NULL;
    }

    if (entry->value != NULL) {
        rps_free(entry->value);
        entry->value = NULL;
    }

    entry->key_size = 0;
    entry->value_size = 0;
    entry->next = NULL;
}

static void
hashmap_entry_destroy(struct hash_entry *entry) {
    hashmap_entry_deinit(entry);
    rps_free(entry);
}

int
hashmap_init(rps_hashmap_t *map, uint32_t nbuckets) {
    uint32_t i;
    struct hash_entry **buckets;

    ASSERT(map != NULL);

    map->seed = (uint32_t)rps_random(MAX_SEED);
    map->size = nbuckets;

    buckets = rps_alloc(map->size * sizeof(*buckets));
    if (buckets == NULL) {
        log_error("create hash table failed to allocate memory");
        return RPS_ENOMEM;
    }

    map->buckets = buckets;

    for (i = 0; i < map->size; i++) {
        map->buckets[i] = NULL;
    }

    /* murmurhash3 32bit throughput is good enough in our approach */
    map->hashfunc = MurmurHash3_x86_32;

    return RPS_OK;
}

/*

void
hashmap_deinit(rps_hashmap_t *map) {
    uint32_t i;
    struct hash_entry *entry;

    ASSERT(map != NULL);
    
    for (i = 0; i < map->size; i++) {
        entry = map->buckets[i];
        if (entry != NULL) {
            rps_free(entry);
        }
        
    }
}
*/

rps_hashmap_t *
hashmap_create(uint32_t nbuckets) {
    rps_hashmap_t *map;

    ASSERT(nbuckets >0);

    map = (rps_hashmap_t *) rps_alloc(sizeof(struct rps_hashmap_s));
    if (map == NULL) {
        return NULL;
    }

    if (hashmap_init(map, nbuckets) != RPS_OK) {
        return NULL;
    }

    return map;
}


void
hashmap_put(rps_hashmap_t *map, void *key, size_t key_size, void *value, size_t value_size) {
    struct hash_entry *entry;

    entry = hashmap_entry_create(key, key_size, value, value_size);
}
