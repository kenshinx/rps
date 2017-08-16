#ifndef _UPSTREAM_H
#define _UPSTREAM_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "queue.h"
#include "_string.h"
#include "config.h"

#include <uv.h>

#define UPSTREAM_DEFAULT_WEIGHT 10
#define UPSTREAM_DEFAULT_POOL_LENGTH 1000
#define UPSTREAM_DEFAULT_TIME_WHEEL_LENGTH 1000
#define UPSTREAM_DEFAULT_SCHEDULE up_rr

#define UPSTREAM_MIN_FAILURE   10
#define UPSTREAM_MAX_LOOP      10

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
    rps_str_t   source;

    uint16_t    weight;
    uint32_t    success;
    uint32_t    failure;
    uint32_t    count;

    rps_ts_t    insert_date;

    /* The time wheel which be used to control the QPS
     * The element storaged in queue are long int expressed timestamp, 
     * 4 bytes in 32bit platform, 8 bytes in 64 bits which exactly the pointer length on various platform.
     */
    rps_queue_t timewheel;
    
    uint8_t     enable:1;
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
    uint32_t                mr1m;
    uint32_t                mr1h;
    uint32_t                mr1d;
    float                   max_fail_rate;
    rps_array_t             pools;
    uv_cond_t               ready;
    uv_mutex_t              mutex;
    uint8_t                 once:1;
};

rps_status_t upstream_init(struct upstream *u);
void upstream_deinit(struct upstream *u);

rps_status_t upstreams_init(struct upstreams *us, 
        struct config_api *api, struct config_upstreams *cu);
struct upstream  *upstreams_get(struct upstreams *us, rps_proto_t proto);
void upstreams_deinit(struct upstreams *us);
void upstreams_refresh(uv_timer_t *handle);

#endif
