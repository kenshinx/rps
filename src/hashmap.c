#include "hashmap.h"
#include "murmur3.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>

//max int32 
#define MAX_SEED  2147483647 

static struct hashmap_entry *
hashmap_entry_create(void *key, size_t key_size, void *value, size_t value_size) {
    struct hashmap_entry *entry;

    ASSERT(key_size > 0);
    ASSERT(value_size > 0);

    entry = rps_alloc(sizeof(struct hashmap_entry));
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
hashmap_entry_deinit(struct hashmap_entry *entry) {

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

static bool
hashmap_entry_compare(struct hashmap_entry *e1, struct hashmap_entry *e2) {
    if (e1->key_size != e2->key_size) {
        return false;
    }

    return (memcmp(e1->key, e2->key, e1->key_size) == 0);
}

static void
hashmap_entry_set(struct hashmap_entry *entry, void *value, size_t value_size) {
    if (entry->value != NULL) {
        rps_free(entry->value);
    }

    entry->value = rps_alloc(value_size);
    if (entry->value == NULL) {
        log_error("update hashmap entry alloc memory failed");
        return;
    }

    memcpy(entry->value, value, value_size);
    entry->value_size = value_size;
}

static void
hashmap_entry_destroy(struct hashmap_entry *entry) {
    hashmap_entry_deinit(entry);
    rps_free(entry);
}


static uint32_t
hashmap_index(rps_hashmap_t *map, void *key, size_t key_size) {
    uint32_t index;

    map->hashfunc(key, key_size, map->seed, &index);

    index %= map->size;

    return index;
}

int
hashmap_init(rps_hashmap_t *map, uint32_t nbuckets, double max_load_factor) {
    uint32_t i;
    struct hashmap_entry **buckets;

    ASSERT(map != NULL);

    map->seed = (uint32_t)rps_random(MAX_SEED);
    map->size = nbuckets;
    map->count = 0;
    map->collisions = 0;
    map->max_load_factor= max_load_factor;

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
hashmap_create(uint32_t nbuckets, double max_load_factor) {
    rps_hashmap_t *map;

    ASSERT(nbuckets >0);

    map = (rps_hashmap_t *) rps_alloc(sizeof(struct rps_hashmap_s));
    if (map == NULL) {
        return NULL;
    }

    if (hashmap_init(map, nbuckets, max_load_factor) != RPS_OK) {
        return NULL;
    }

    return map;
}

void
hashmap_deinit(rps_hashmap_t *map) {
    uint32_t i;
    struct hashmap_entry *entry;
    struct hashmap_entry *tmp;

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

    map->buckets = NULL;
    map->size = 0;
    map->seed = 0;
    map->count = 0;
    map->max_load_factor = 0;
    map->hashfunc = NULL;
}

static void
hashmap_set_entry(rps_hashmap_t *map, struct hashmap_entry *entry) {
    uint32_t index;
    struct hashmap_entry *tmp;
    double current_load_factor;
    
    index = hashmap_index(map, entry->key, entry->key_size);

    tmp = map->buckets[index];

    if (tmp == NULL) {  /* no collision */
        map->buckets[index] = entry;
        map->count++;
        return;
    }

    while (tmp->next != NULL) {
        if (hashmap_entry_compare(tmp, entry)) {
            break;
        }
        tmp = tmp->next;
    }

    /* the keys are identical, throw away the old entry 
     * and update new value.
     */
    if(hashmap_entry_compare(tmp, entry)) {
        hashmap_entry_set(tmp, entry->value, entry->value_size);
        hashmap_entry_destroy(entry);
        return;
    } 

    /* append new entry onto the end of the link */
    tmp->next = entry;
    map->collisions++;
    map->count++;

    current_load_factor = (double)(map->collisions/map->size);
    if (current_load_factor > map->max_load_factor) {
        hashmap_rehash(map, map->size * 2);
    }

}

void
hashmap_rehash(rps_hashmap_t *map, uint32_t new_size) {
    rps_hashmap_t new_map;
    struct hashmap_entry *entry;
    struct hashmap_entry *next;
    uint32_t i;

    ASSERT(new_size > map->size);

    hashmap_init(&new_map, new_size, map->max_load_factor);

    for (i = 0; i < map->size; i++) {
        entry = map->buckets[i];
        if (entry == NULL) {
            continue;
        }

        while (entry != NULL) {
            next = entry->next;
            hashmap_set_entry(&new_map, entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }

    hashmap_deinit(map);

    map->buckets = new_map.buckets;
    map->count = new_map.count;
    map->size = new_map.size;
    map->seed = new_map.seed;
    map->collisions = new_map.collisions;
    map->max_load_factor = new_map.max_load_factor;
    map->hashfunc = new_map.hashfunc;
}



void
hashmap_set(rps_hashmap_t *map, void *key, size_t key_size, void *value, size_t value_size) {
    struct hashmap_entry *entry;

    entry = hashmap_entry_create(key, key_size, value, value_size);
    if (entry == NULL) {
        log_error("create hashmap entry failed");
        return;
    }

    hashmap_set_entry(map, entry);
}


void *
hashmap_get(rps_hashmap_t *map, void *key, size_t key_size, size_t *value_size) {
    uint32_t index;
    struct hashmap_entry *entry;
    struct hashmap_entry tmp;

    tmp.key = key;
    tmp.key_size = key_size;

    index = hashmap_index(map, key, key_size);
    entry = map->buckets[index];
    
    while (entry != NULL) {
        if (hashmap_entry_compare(entry, &tmp)) {
            *value_size = entry->value_size;
            return entry->value;
        }
        entry = entry->next;
    }

    *value_size = 0;
    return NULL;
}

int 
hashmap_has(rps_hashmap_t *map, void *key, size_t key_size) {
    void *value;
    size_t value_size;

    value = hashmap_get(map, key, key_size, &value_size);

    return (value != NULL && value_size != 0);
}

int 
hashmap_remove(rps_hashmap_t *map, void *key, size_t key_size) {
    uint32_t index;
    struct hashmap_entry *entry;
    struct hashmap_entry *prev;
    struct hashmap_entry tmp;

    tmp.key = key;
    tmp.key_size = key_size;

    index = hashmap_index(map, key, key_size);

    prev = NULL;
    entry = map->buckets[index];

    while (entry != NULL) {
        if (hashmap_entry_compare(entry, &tmp)) {
            if (prev == NULL) {
                /* head of the link */
                map->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }

            map->count--;
            
            if (prev != NULL) {
                map->collisions--;
            }

            hashmap_entry_destroy(entry);
            return 1;
        } 
        
        prev = entry;
        entry = entry->next;
    }

    return 0;
}
