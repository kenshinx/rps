#include "proxy.h"

#include "util.h"
#include "core.h"
#include "config.h"

#define PROXY_POOL_DEFAULT_LENGTH 64

void
proxy_init(struct proxy *p) {
    UNUSED(p);
}


rps_status_t
proxy_pool_init(struct proxy_pool *pool) {
    rps_status_t status;

    status = array_init(&pool->pool, PROXY_POOL_DEFAULT_LENGTH, sizeof(struct proxy));
    if (status != RPS_OK) {
        return status;
    }

    pool->index = 0;

    return RPS_OK;
}

rps_status_t
proxy_pool_load(struct proxy_pool *pool, struct config_redis *cr) {
    rps_status_t status;
    
}
