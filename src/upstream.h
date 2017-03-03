#ifndef _UPSTREAM_H
#define _UPSTREAM_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "_string.h"
#include "config.h"

#include <uv.h>

#define UPSTREAM_DEFAULT_WEIGHT 10
#define UPSTREAM_DEFAULT_POOL_LENGTH 64
#define UPSTREAM_DEFAULT_SCHEDULE up_rr

#define upstream_pool_is_null(up)  ((up)->pool == NULL)

enum upstream_schedule {
    up_rr,         /* round-robin */
    up_wrr,        /* weighted round-robin*/
    up_random,     /* raondom schedule */
};

struct upstream  {
    rps_addr_t  server;
    rps_str_t   uname;
    rps_str_t   passwd;
    rps_proto_t proto;
    uint16_t    weight;
    uint32_t    count;
};

struct upstream_pool {
    rps_array_t         *pool;
    uint32_t            index;
    uint8_t             schedule;
    uv_rwlock_t         rwlock;
};

void upstream_pool_init(struct upstream_pool *up);
void upstream_pool_deinit(struct upstream_pool *up);
void upstream_pool_dump(struct upstream_pool *up);
rps_status_t upstream_pool_refresh(struct upstream_pool *up, 
        struct config_redis *cr, struct config_upstream *cu);
struct upstream *upstream_pool_get(struct upstream_pool *up);

#endif
