#include "hashtable.h"


rps_hashtable_t *
hashtable_create(uint32_t nbuckets) {
    rps_hashtable_t *ht;
    struct hash_entry **buckets;
    int i;

    ASSERT(nbuckets >0);

    ht = (rps_hashtable_t *) rps_alloc(sizeof(struct rps_hashtable_s));
    if (ht == NULL) {
        return NULL;
    }

    ht->seed = (uint32_t)rand();
    ht->size = nbuckets;

    buckets = rps_alloc(ht->size * sizeof(*buckets));
    if (buckets == NULL) {
        log_error("create hash table failed to allocate memory");
        return NULL;
    }

    ht->buckets = buckets;

    for (i = 0; i < ht->size; i++) {
        ht->buckets = NULL;
    }

    return ht;
    
}
