#include "core.h"
#include "upstream.h"
#include "util.h"
#include "config.h"
#include "_string.h"

#include <uv.h>
#include <hiredis.h>
#include <jansson.h>


static void
upstream_init(struct upstream *u) {
    string_init(&u->uname);   
    string_init(&u->passwd);   
    u->weight = UPSTREAM_DEFAULT_WEIGHT;
    u->count = 0;

}

static void
upstream_deinit(struct upstream *u) {
    string_deinit(&u->uname);
    string_deinit(&u->passwd);
    u->count = 0;
}

#ifdef RPS_DEBUG_OPEN
static void
upstream_str(void *data) {
    char name[MAX_HOSTNAME_LEN];
    struct upstream *u;

    u = (struct upstream *)data;

    rps_unresolve_addr(&u->server, name);
    log_verb("\t%s://%s:%s@%s:%d #%d", rps_proto_str(u->proto), u->uname.data, 
            u->passwd.data, name, rps_unresolve_port(&u->server), u->count);
}
#endif

static rps_status_t
upstream_pool_init(struct upstream_pool *up, struct config_upstream *cu, 
        struct config_redis *cr) {
    up->index = 0;
    up->cr = cr;
    up->pool = NULL;
    uv_rwlock_init(&up->rwlock);

    up->proto = rps_proto_int((const char *)cu->proto.data);
    if (up->proto < 0) {
        log_error("unsupport proto:%s", cu->proto.data);
        return RPS_ERROR;
    }
    
    string_init(&up->rediskey);
    if (string_copy(&up->rediskey, &cu->rediskey) != RPS_OK) {
        return RPS_ERROR;
    }

    up->pool = array_create(UPSTREAM_DEFAULT_POOL_LENGTH, sizeof(struct upstream));
    if (up->pool == NULL) {
        return RPS_ERROR;
    }

    return RPS_OK;
}


static void
upstream_pool_deinit(struct upstream_pool *up) {
    if (up->pool != NULL) {
        while(array_n(up->pool)) {
            upstream_deinit((struct upstream *)array_pop(up->pool));
        }
        array_destroy(up->pool);
    }
    up->pool = NULL;
    up->index = 0;
    up->cr = NULL;
    uv_rwlock_destroy(&up->rwlock);
} 

#ifdef RPS_DEBUG_OPEN
static void
upstream_pool_dump(struct upstream_pool *up) {
    log_verb("[rps upstream proxy pool]");
    array_foreach(up->pool, upstream_str);
}
#endif

rps_status_t 
upstreams_init(struct upstreams *us, struct config_redis *cr, 
        struct config_upstreams *cus) {

    rps_status_t status;
    rps_str_t   *schedule;
    int i, len;
    struct upstream_pool *up;
    struct config_upstream *cu;

    us->hybrid = cus->hybrid;   
    us->maxretry = cus->maxretry;

    schedule = &cus->schedule;
    if (rps_strcmp(schedule, "rr") == 0) {
        us->schedule = up_rr;
    } else if (rps_strcmp(schedule, "random") == 0) {
        us->schedule = up_random;
    } else if (rps_strcmp(schedule, "wrr") == 0) {
        log_error("wrr algorithm have not implemented");
        abort();
    } else {
        NOT_REACHED();
    }

    len = array_n(cus->pools);

    status = array_init(&us->pools, len, sizeof(struct upstream_pool));
    if (status != RPS_OK) {
        return status;
    }
    
    for (i=0; i<len; i++) {
        up = (struct upstream_pool *)array_push(&us->pools);
        cu = (struct config_upstream *)array_get(cus->pools, i);
        
        if (upstream_pool_init(up, cu, cr) != RPS_OK) {
            goto error;
        }
    }

    return  RPS_OK;

error:
    while(array_n(&us->pools)) {
        upstream_pool_deinit((struct upstream_pool *)array_pop(&us->pools));
    }
    
    log_error("upstreams init failed");
    return RPS_ERROR;
}

void 
upstreams_deinit(struct upstreams *us) {
    while(array_n(&us->pools)) {
        upstream_pool_deinit((struct upstream_pool *)array_pop(&us->pools));
    }
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
		redisFree(c);
        return NULL;
    }

    freeReplyObject(reply);
 
    log_debug("connect redis %s:%d success", cfg->host.data, cfg->port);

    return c;
}

static rps_status_t
upstream_json_parse(const char *str, struct upstream *u) {
    json_t *root;
    json_error_t error;
    rps_str_t host;
    uint16_t port;
    void *kv;
    rps_status_t status;

    string_init(&host);

    root = json_loads(str, 0, &error);
    if (!root) {
        log_error("json decode '%s' error: %s", str, error.text);
        return RPS_ERROR;
    }
    
    for (kv = json_object_iter(root); kv; kv = json_object_iter_next(root, kv)) {
        status = RPS_OK;
        json_t *tmp = json_object_iter_value(kv);
        if (strcmp(json_object_iter_key(kv), "host") == 0 && 
                json_typeof(tmp) == JSON_STRING) {
            status = string_duplicate(&host, json_string_value(tmp), json_string_length(tmp));
        } else if (strcmp(json_object_iter_key(kv), "port") == 0 && 
                json_typeof(tmp) == JSON_INTEGER) {
            port = (uint16_t)json_integer_value(tmp);
        } else if (strcmp(json_object_iter_key(kv), "proto") == 0 && 
                json_typeof(tmp) == JSON_STRING) {
            u->proto = rps_proto_int(json_string_value(tmp));
            if (u->proto < 0) {
                status = RPS_ERROR;
            }
        } else if (strcmp(json_object_iter_key(kv), "username") == 0) {
            /* Ignore username is null */
            if (json_typeof(tmp) == JSON_STRING) {
                status = string_duplicate(&u->uname, json_string_value(tmp), json_string_length(tmp));
            }
        } else if (strcmp(json_object_iter_key(kv), "password") == 0) {
            /* Ignore password is null */
            if (json_typeof(tmp) == JSON_STRING) {
                status = string_duplicate(&u->passwd, json_string_value(tmp), json_string_length(tmp));
            }
        } else if (strcmp(json_object_iter_key(kv), "weight") == 0) {
            u->weight = (uint16_t)json_integer_value(tmp);
        } else {
            status = RPS_ERROR;
        }

        if (status != RPS_OK) {
            log_error("json parse '%s:%s' error", json_object_iter_key(kv), json_string_value(tmp));
            return status;
        }
    }

    status = rps_resolve_inet((const char *)host.data, port, &u->server);
    if (status != RPS_OK) {
        log_error("jason parse error, invalid upstream address, %s:%d", host.data, port);
        return status;
    }

    return RPS_OK;

}

static rps_status_t
upstream_pool_load(rps_array_t *pool, struct config_redis *cr, rps_str_t *rediskey) {
    redisContext *c;
    redisReply  *reply;
    struct upstream *upstream;
    size_t i;

    c =  upstream_redis_connect(cr);
    if (c == NULL) {
        return RPS_ERROR;
    }

    reply = redisCommand(c, "SMEMBERS %s", rediskey->data);
    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) {
            freeReplyObject(reply);
        }
        log_error("redis get upstreams failed.");	
		redisFree(c);
        return RPS_ERROR;
    }

	redisFree(c);

    for (i = 0; i < reply->elements; i++) {
        upstream = (struct upstream *)array_push(pool);
        upstream_init(upstream);
        if (upstream_json_parse(reply->element[i]->str, upstream) != RPS_OK) {
            array_pop(pool);
            upstream_deinit(upstream);
        }
    }

    return RPS_OK;
}

static rps_status_t
upstream_pool_refresh(struct upstream_pool *up) {

    rps_array_t *new_pool;

    /* Free current upstream pool only when new pool load successful */

    new_pool = array_create(UPSTREAM_DEFAULT_POOL_LENGTH, sizeof(struct upstream));
    if (new_pool == NULL) {
        return RPS_ERROR;
    }


    if (upstream_pool_load(new_pool, up->cr, &up->rediskey) != RPS_OK) {
        while(array_n(new_pool)) {
            upstream_deinit((struct upstream *)array_pop(new_pool));
        }
        array_destroy(new_pool);
        log_error("load %s upstreams from redis failed.", rps_proto_str(up->proto));
        return RPS_ERROR;
    }

    uv_rwlock_wrlock(&up->rwlock);
    array_swap(&up->pool, &new_pool);
    uv_rwlock_wrunlock(&up->rwlock);
    
    if (new_pool != NULL) {
        while(array_n(new_pool)) {
            upstream_deinit((struct upstream *)array_pop(new_pool));
        }
        array_destroy(new_pool);
    }
    

    #ifdef RPS_DEBUG_OPEN
        upstream_pool_dump(up);
    #endif

    return RPS_OK;
}

void
upstreams_refresh(uv_timer_t *handle) {
    struct upstreams *us;
    struct upstream_pool *up;
    int i, len;
    const char *proto;

    us = (struct upstreams *)handle->data;

    len = array_n(&us->pools);

    for (i=0; i< len; i++) {
        up = (struct upstream_pool *)array_get(&us->pools, i);

        proto = rps_proto_str(up->proto);

        if (upstream_pool_refresh(up) != RPS_OK) { 
            log_error("update %s upstream proxy pool failed", proto) ;
    log_debug("");
        } else {
            log_debug("refresh %s upstream pool, get <%d> proxys", proto, array_n(up->pool));
        }
    }
}

static struct upstream *
upstream_pool_get_rr(struct upstream_pool *up) {
    struct upstream *upstream;

    if (up->pool == NULL) {
        log_error("upstream pool is null");
        return NULL;
    }
    
    if (up->index >= array_n(up->pool)) {
        up->index = 0;
    }

    upstream = array_get(up->pool, up->index++);

    return upstream;
}

static struct upstream *
upstream_pool_get_random(struct upstream_pool *up) {
    struct upstream *upstream;
    int i;

    if (up->pool == NULL) {
        log_error("upstream pool is null");
        return NULL;
    }

    if (array_is_empty(up->pool)) {
        log_error("upstream pool is null");
        return NULL;
    }

    i = rps_random(array_n(up->pool));
    
    upstream = array_get(up->pool, i);
    up->index = i;

    return upstream;
    
}

rps_status_t
upstreams_get(struct upstreams *us, rps_proto_t proto, struct upstream *u) {
    struct upstream *upstream;
    struct upstream_pool *up;
    int i, len;

    upstream = NULL;
    up = NULL;

    if (us->hybrid) {
        up = array_random(&us->pools);
    } else {
        len = array_n(&us->pools);
        for (i=0; i<len; i++) {
            up = array_get(&us->pools, i);
            if (up->proto == proto) {
                break;
            }
        }
    }

    uv_rwlock_rdlock(&up->rwlock);

    switch (us->schedule) {
        case up_rr:
            upstream = upstream_pool_get_rr(up);
            break;
        case up_random:
            upstream = upstream_pool_get_random(up);       
            break;
        case up_wrr:
        default:
            NOT_REACHED();
    }   

    if (upstream == NULL) {
        uv_rwlock_rdunlock(&up->rwlock);
        return RPS_EUPSTREAM;
    }

    upstream->count++;
    memcpy(u, upstream, sizeof(struct upstream));

#if RPS_DEBUG_OPEN
    upstream_str(upstream);
#endif
    
    uv_rwlock_rdunlock(&up->rwlock);

    return RPS_OK;
}
