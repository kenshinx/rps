#include "hashtable.h"
#include "murmur3.h"
#include "core.h"

#include "stdlib.h"


#define MAX_SEED  2147483647

int
hashtable_init(rps_hashtable_t *ht, uint32_t nbuckets) {
    uint32_t i;
    struct hash_entry **buckets;

    ASSERT(ht != NULL);

    ht->seed = (uint32_t)rps_random(MAX_SEED);
    ht->size = nbuckets;

    buckets = rps_alloc(ht->size * sizeof(*buckets));
    if (buckets == NULL) {
        log_error("create hash table failed to allocate memory");
        return RPS_ENOMEM;
    }

    ht->buckets = buckets;

    for (i = 0; i < ht->size; i++) {
        ht->buckets[i] = NULL;
    }

    /* murmurhash3 32bit throughput is good enough in our approach */
    ht->hashfunc = MurmurHash3_x86_32;

    return RPS_OK;
}

/*

void
hashtable_deinit(rps_hashtable_t *ht) {
    uint32_t i;
    struct hash_entry *entry;

    ASSERT(ht != NULL);
    
    for (i = 0; i < ht->size; i++) {
        entry = ht->buckets[i];
        if (entry != NULL) {
            rps_free(entry);
        }
        
    }
}
*/

rps_hashtable_t *
hashtable_create(uint32_t nbuckets) {
    rps_hashtable_t *ht;

    ASSERT(nbuckets >0);

    ht = (rps_hashtable_t *) rps_alloc(sizeof(struct rps_hashtable_s));
    if (ht == NULL) {
        return NULL;
    }

    if (hashtable_init(ht, nbuckets) != RPS_OK) {
        return NULL;
    }

    return ht;
}

static struct hash_entry *
hashtable_entry_create(void *key, size_t key_size, void *value, size_t value_size) {
    struct hash_entry *he;

    ASSERT(key_size > 0);
    ASSERT(value_size > 0);

    he = rps_alloc(sizeof(struct hash_entry));
    if (he == NULL) {
        return NULL;
    }
    
    he->key = rps_alloc(key_size);
    if (he->key == NULL) {
        rps_free(he);
        return NULL;
    }
    he->key_size = key_size;
    memcpy(he->key, key, key_size);

    he->value = rps_alloc(value_size);
    if (he->value == NULL) {
        rps_free(he->key);
        rps_free(he);
        return NULL;
    }
    he->value_size = value_size;
    memcpy(he->value, value, value_size);

    he->next = NULL;

    return he;
}



void
hashtable_set(rps_hashtable_t *ht, rps_str_t *key, void *value, size_t size) {
    struct hash_entry *he;

    he = hashtable_entry_create(key, vlaue, size);
}
