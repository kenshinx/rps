#include "proxy.h"
#include "util.h"
#include "core.h"
#include "config.h"
#include "_string.h"

#include <hiredis.h>

#define PROXY_POOL_DEFAULT_LENGTH 64

static void
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

static redisContext *
proxy_redis_connect(struct config_redis *cfg) {
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

    if (!string_empty(&cfg->password)) {
        reply= redisCommand(c, "AUTH %s", cfg->password.data);
    }
    

    if (reply->type == REDIS_REPLY_ERROR) {
        log_error("redis authentication failed.");
        freeReplyObject(reply);
        return NULL;
    }

    freeReplyObject(reply);
 
    log_debug("connect redis %s:%d success", cfg->host.data, cfg->port);

    return c;
}

rps_status_t
proxy_pool_load(struct proxy_pool *pool, struct config_redis *cfg) {
    rps_status_t status;
    redisContext *c;

    c =  proxy_redis_connect(cfg);
    if (c == NULL) {
        return RPS_ERROR;
    }

    return RPS_OK;
}
