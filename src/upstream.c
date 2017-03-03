#include "upstream.h"
#include "util.h"
#include "core.h"
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

static void
upstream_str(void *data) {
    char name[MAX_HOSTNAME_LEN];
    struct upstream *u;

    u = (struct upstream *)data;

    rps_unresolve_addr(&u->server, name);
    log_verb("\t%s://%s:%s@%s:%d ", rps_proto_str(u->proto), 
            u->uname.data, u->passwd.data, name, rps_unresolve_port(&u->server));
}

void
upstream_pool_init(struct upstream_pool *up) {
    up->pool = NULL;
    up->index = 0;
    up->schedule = UPSTREAM_DEFAULT_SCHEDULE;
    uv_rwlock_init(&up->rwlock);
}

static rps_status_t
upstream_pool_setup(struct upstream_pool *up, struct config_upstream *cu) {
    ASSERT(up->pool == NULL);

    up->pool = array_create(UPSTREAM_DEFAULT_POOL_LENGTH, sizeof(struct upstream));
    if (up->pool == NULL) {
        return RPS_ERROR;
    }

    if (rps_strcmp(&cu->schedule, "rr") == 0) {
        up->schedule = up_rr;
    } else if (rps_strcmp(&cu->schedule, "random") == 0) {
        up->schedule = up_random;
    } else if (rps_strcmp(&cu->schedule, "wrr") == 0) {
        log_error("wrr algorithm have not implemented");
        abort();
    } else {
        NOT_REACHED();
    }


    return RPS_OK;
}

static void
upstream_pool_teardown(struct upstream_pool *up) {
	while(array_n(up->pool)) {
		upstream_deinit((struct upstream *)array_pop(up->pool));
	}
    array_destroy(up->pool);
}

void
upstream_pool_deinit(struct upstream_pool *up) {
    upstream_pool_teardown(up);
    up->pool = NULL;
    up->index = 0;
    uv_rwlock_destroy(&up->rwlock);
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
        log_error("jason parse error, invalid upstream address, %s:%d", host, port);
		/* Avoid single invalid record  result in process exit */
		// return status;  
    }

    return RPS_OK;

}

static rps_status_t
upstream_pool_load(struct upstream_pool *up, 
        struct config_redis *cr, struct config_upstream *cu) {
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
		redisFree(c);
        return RPS_ERROR;
    }

	redisFree(c);

    for (i = 0; i < reply->elements; i++) {
        upstream = (struct upstream *)array_push(up->pool);
        upstream_init(upstream);
        if (upstream_json_parse(reply->element[i]->str, upstream) != RPS_OK) {
            return RPS_ERROR;
        }
    }

    return RPS_OK;
}

rps_status_t
upstream_pool_refresh(struct upstream_pool *up, 
        struct config_redis *cr, struct config_upstream *cu) {

    struct upstream_pool new_up;

    /* Free current upstream pool only when new pool load successful */

    upstream_pool_init(&new_up);

    if (upstream_pool_setup(&new_up, cu) != RPS_OK) {
        log_error("setup new upstream pool failed");
        return RPS_ERROR;
    }

    if (upstream_pool_load(&new_up, cr, cu) != RPS_OK) {
        upstream_pool_deinit(&new_up);
        log_error("load upstreams from redis failed.");
        return RPS_ERROR;
    }

    uv_rwlock_wrlock(&up->rwlock);
    array_swap(&up->pool, &new_up.pool);
    uv_rwlock_wrunlock(&up->rwlock);

    if (!upstream_pool_is_null(&new_up)) {
        upstream_pool_deinit(&new_up);
    }
    

    log_debug("refresh upstream pool, get <%d> proxys", array_n(up->pool));

    return RPS_OK;
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

    uv_rwlock_rdlock(&up->rwlock);
    upstream = array_get(up->pool, up->index++);
    uv_rwlock_rdunlock(&up->rwlock);

    upstream_str(upstream);

    return upstream;
}

static struct upstream *
upstream_pool_get_random(struct upstream_pool *up) {
    
}

struct upstream *
upstream_pool_get(struct upstream_pool *up) {
    struct upstream *upstream;

    upstream = NULL;

    switch (up->schedule) {
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
    
    return upstream;
}

void
upstream_pool_dump(struct upstream_pool *up) {
    log_verb("[rps upstream proxy pool]");
    array_foreach(up->pool, upstream_str);
}

