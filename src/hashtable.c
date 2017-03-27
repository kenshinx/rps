#include "hashtable.h"
#include "murmur3.h"
#include "core.h"


int
hashtable_init(rps_hashtable_t *ht, uint32_t nbuckets) {
    uint32_t i;
    struct hash_entry **buckets;

    ASSERT(ht != NULL);

    ht->seed = (uint32_t)rand();
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
