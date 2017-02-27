#ifndef _PROXY_H
#define _PROXY_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "_string.h"
#include "config.h"

struct proxy  {
    rps_addr_t  server;
    rps_str_t   uname;
    rps_str_t   passwd;
    rps_proto_t proto;
    uint16_t    weight;
};

struct proxy_pool {
    rps_array_t pool;
    uint32_t    index;
};

rps_status_t proxy_pool_init(struct proxy_pool *pool);
rps_status_t proxy_pool_load(struct proxy_pool *pool, struct config_redis *cr);

#endif
