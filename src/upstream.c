#include "upstream.h"
#include "util.h"
#include "core.h"
#include "config.h"
#include "_string.h"

#include <hiredis.h>

#define UPSTREAM_POOL_DEFAULT_LENGTH 64

static void
upstream_init(struct upstream *p) {
    UNUSED(p);
}



rps_status_t
upstream_pool_init(struct upstream_pool *up) {
    rps_status_t status;

    status = array_init(&up->pool, UPSTREAM_POOL_DEFAULT_LENGTH, sizeof(struct upstream));
    if (status != RPS_OK) {
        return status;
    }

    up->index = 0;

    return RPS_OK;
}

static redisContext *
upstream_redis_connect(struct config_redis *cfg) {
    redisContext *c;
    redisReply  *reply;
    
    struct timeval timeout = {cfg->timeout, 0};

    c = redisConnectWithTimeout((const char *)cfg->host.data, (int)cfg->port, timeout);

    if (c == NULL || c->err) { 
        if (c) {
            log_error("connect redis %s:%d failed: %s\n", cfg->host.data, cfg->port, c->errstr);
            redisFree(c);
        } else {
            log_error("connect redis %s:%d failed, can't allocate redis context",
                    cfg->host.data, cfg->port);
        }

        return NULL;
    }

    if (string_empty(&cfg->password)) {
        log_debug("connect redis %s:%d success", cfg->host.data, cfg->port);
        return c;
    }
    
    reply= redisCommand(c, "AUTH %s", cfg->password.data);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        if (reply) {
            freeReplyObject(reply);
        }
        log_error("redis authentication failed.");
        return NULL;
    }

    freeReplyObject(reply);
 
    log_debug("connect redis %s:%d success", cfg->host.data, cfg->port);

    return c;
}

static rps_status_t
upstream_json_parse(const char *str, struct upstream *upstream) {
    printf("%s\n", str);

    return RPS_OK;

}

rps_status_t
upstream_pool_load(struct upstream_pool *up, struct config_redis *cr, struct config_upstream *cu) {
    redisContext *c;
    redisReply  *reply;
    struct upstream *upstream;
    size_t i;

    c =  upstream_redis_connect(cr);
    if (c == NULL) {
        return RPS_ERROR;
    }

    reply = redisCommand(c, "SMEMBERS %s", cu->rediskey.data);
    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) {
            freeReplyObject(reply);
        }
        log_error("redis get upstreams failed.");
        return RPS_ERROR;
    }

    for (i = 0; i < reply->elements; i++) {
        upstream = array_push(&up->pool);
        upstream_json_parse(reply->element[i]->str, upstream);
    }

    return RPS_OK;
}
