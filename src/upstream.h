#ifndef _UPSTREAM_H
#define _UPSTREAM_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "_string.h"
#include "config.h"

#define UPSTREAM_DEFAULT_WEIGHT 10
#define UPSTREAM_DEFAULT_POOL_LENGTH 64

struct upstream  {
    rps_addr_t  server;
    rps_str_t   uname;
    rps_str_t   passwd;
    rps_proto_t proto;
    uint16_t    weight;
};

struct upstream_pool {
    rps_array_t *pool;
    uint32_t    index;
};

rps_status_t upstream_pool_init(struct upstream_pool *up);
void upstream_pool_deinit(struct upstream_pool *up);
void upstream_pool_dump(struct upstream_pool *up);
rps_status_t upstream_pool_refresh(struct upstream_pool *up, 
        struct config_redis *cr, struct config_upstream *cu);

#endif
