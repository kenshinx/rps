#include "hashmap.h"
#include "murmur3.h"
#include "core.h"

#include "stdio.h"
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

    ASSERT(entry != NULL);

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
    map->count = 0;

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
hashmap_destroy(rps_hashmap_t *map) {
    uint32_t i;
    struct hash_entry *entry;
    struct hash_entry *tmp;

    ASSERT(map != NULL);
    
    for (i = 0; i < map->size; i++) {
        entry = map->buckets[i];
        while (entry != NULL) {
            tmp = entry->next;
            hashmap_entry_destroy(entry);
            entry = tmp;
        }
        
        map->buckets[i] = NULL;
    }

    rps_free(map->buckets);

    map->size = 0;
    map->seed = 0;
    map->count = 0;
    map->hashfunc = NULL;
}

static uint32_t
hashmap_index(rps_hashmap_t *map, void *key, size_t key_size) {
    uint32_t index;

    map->hashfunc(key, key_size, map->seed, &index);

    index %= map->size;

    return index;
}

void
hashmap_set(rps_hashmap_t *map, void *key, size_t key_size, void *value, size_t value_size) {
    uint32_t index;
    struct hash_entry *entry;
    struct hash_entry *tmp;

    entry = hashmap_entry_create(key, key_size, value, value_size);
    index = hashmap_index(map, key, key_size);

    tmp = map->buckets[index];
    
    printf("key:%s, index:%d\n", key, index);

    if (tmp == NULL) {
        map->buckets[index] = entry;
        map->count++;
        return;
    }


    
}
