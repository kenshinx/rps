#ifndef _UPSTREAM_H
#define _UPSTREAM_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "_string.h"
#include "config.h"

#include <uv.h>

#define UPSTREAM_DEFAULT_WEIGHT 10
#define UPSTREAM_DEFAULT_POOL_LENGTH 1000
#define UPSTREAM_DEFAULT_SCHEDULE up_rr

enum upstream_schedule {
    up_rr,         /* round-robin */
    up_wrr,        /* weighted round-robin*/
    up_random,     /* raondom schedule */
};

/*
 * upstreams.pools -> {2-3}upstream_pool.pool -> {n}upstream
 */

struct upstream  {
    rps_addr_t  server;
    rps_proto_t proto;
    rps_str_t   uname;
    rps_str_t   passwd;
    uint16_t    weight;
    uint32_t    count;
};

struct upstream_pool {
    rps_array_t             *pool;
    rps_proto_t             proto;
    rps_str_t               api;
    uint32_t                timeout; //api request max timeout
    uint32_t                index;
    uv_rwlock_t             rwlock;
};

struct upstreams {
    uint8_t                 schedule;
    bool                    hybrid;
    uint16_t                maxreconn;
    uint16_t                maxretry;
    rps_array_t             pools;
    uv_cond_t               ready;
    uv_mutex_t              mutex;
    uint8_t                 once:1;
};

void upstream_init(struct upstream *u);
void upstream_deinit(struct upstream *u);

rps_status_t upstreams_init(struct upstreams *us, 
        struct config_api *api, struct config_upstreams *cu);
rps_status_t upstreams_get(struct upstreams *us, rps_proto_t proto, 
        struct upstream *u);
void upstreams_deinit(struct upstreams *us);
void upstreams_refresh(uv_timer_t *handle);

#endif
