#include "core.h"
#include "upstream.h"
#include "util.h"
#include "config.h"
#include "_string.h"

#include <uv.h>
#include <jansson.h>
#include <curl/curl.h>

typedef struct upstream * (*upstream_pool_get_algorithm)(struct upstream_pool *);

struct curl_buf {
    uint8_t *buf;
    size_t  len;
};

void
upstream_init(struct upstream *u) {
    string_init(&u->uname);   
    string_init(&u->passwd);   
    string_init(&u->source);
    u->weight = UPSTREAM_DEFAULT_WEIGHT;
    u->proto = UNSUPPORT;
    u->success = 0;
    u->failure = 0;
    u->count = 0;
    u->insert_date = 0;
    u->enable = 0;

    queue_null(&u->timewheel);
}

void
upstream_deinit(struct upstream *u) {
    string_deinit(&u->uname);
    string_deinit(&u->passwd);
    string_deinit(&u->source);
    u->success = 0;
    u->failure = 0;
    u->count = 0;
    u->insert_date = 0;

    if (!queue_is_null(&u->timewheel)) {
        queue_deinit(&u->timewheel);
    }
}

static rps_status_t
upstream_init_timewheel(struct upstream *u, uint32_t mr1m, uint32_t mr1h, uint32_t mr1d) {
    uint32_t n;

    n = MAX(MAX(mr1m, mr1h), mr1d);
    
    n = n > 0 ? n : UPSTREAM_DEFAULT_TIME_WHEEL_LENGTH;

    printf("n: %d\n", n);

    return queue_init(&u->timewheel, n);
}


/*
static void
upstream_copy(struct upstream *dst, struct upstream *src) {
    dst->proto = src->proto;
    dst->weight = src->weight;

    memcpy(&dst->server, &src->server, sizeof(src->server));
    if (!string_empty(&src->uname)) {
        string_copy(&dst->uname, &src->uname);
    }
    if (!string_empty(&src->passwd)) {
        string_copy(&dst->passwd, &src->passwd);
    }
}
*/

#ifdef RPS_DEBUG_OPEN
static void
upstream_str(void *data) {
    char name[MAX_HOSTNAME_LEN];
    struct upstream *u;

    u = (struct upstream *)data;

    rps_unresolve_addr(&u->server, name);
    log_verb("\t%s://%s:%s@%s:%d (s:%d, f:%d, c:%d, d:%d)", rps_proto_str(u->proto), 
            u->uname.data, u->passwd.data, name, rps_unresolve_port(&u->server), 
            u->success, u->failure, u->count, queue_n(&u->timewheel));
}
#endif

static bool
upstream_fresh(struct upstream *u) {
    return queue_is_null(&u->timewheel);
}

static bool
upstream_poor_quality(struct upstream *u, float max_fail_rate) {
    float fail_rate;

    if (u->failure <= UPSTREAM_MIN_FAILURE) {
        return false;
    }

    if (max_fail_rate == 0.0) {
        //ignore max_fail_rate if be setted to 0
        return false;
    }

    fail_rate = (u->failure/(float)(u->failure + u->success));

    return fail_rate > max_fail_rate;
}


static bool
upstream_request_too_often(struct upstream *u, uint32_t mr1m, uint32_t mr1h, uint32_t mr1d) {
    uint32_t m, h, d;   
    uint32_t i, j, n;
    rps_ts_t now, m_ago, h_ago, d_ago;
    rps_ts_t ts;
    rps_ts_t deadline;
    rps_queue_t *timewheel;

    m = 0;
    h = 0;
    d = 0;

    now = time(NULL);
    m_ago = now - 60;
    h_ago = now - 60 * 60;
    d_ago = now - 60 * 60 * 24;

    timewheel = &u->timewheel;

    i = timewheel->head;
    n = queue_n(timewheel);
    
    if (mr1d > 0) {
        deadline = d_ago;
    } else if (mr1h > 0) {
        deadline = h_ago;
    } else if (mr1m > 0) {
        deadline = m_ago;
    } else {
        deadline = now;
    }
    
    for (j = 0; j < n; j++) {
        ts = (rps_ts_t)timewheel->elts[i];
        i = (i + 1) % timewheel->nelts;

        //remove the ts older than deadline.
        if (ts < deadline) {
            queue_de(timewheel);
            continue;   
        }

        if (ts > m_ago) {
            m += 1;
            h += 1;
            continue;
        }

        if (ts > h_ago) {
            h += 1;
        }
    }

    d = queue_n(timewheel); 

    printf("m:%d, h:%d, d:%d\n", m, h, d);

    return ((mr1m != 0 && m >= mr1m) || 
            (mr1h != 0 && h >= mr1h) || 
            (mr1d != 0 && d >= mr1d));
}

static void
upstream_timewheel_add(struct upstream *u) {
    rps_ts_t now;
    
    now = time(NULL);

    queue_en(&u->timewheel, (void *)now);
}

static rps_status_t
upstream_pool_init(struct upstream_pool *up, struct config_upstream *cu, 
        struct config_api *capi) {
    char api[MAX_API_LENGTH];

    up->index = 0;
    up->pool = NULL;
    up->timeout = capi->timeout;
    uv_rwlock_init(&up->rwlock);

    up->proto = rps_proto_int((const char *)cu->proto.data);
    if (up->proto < 0) {
        log_error("unsupport proto:%s", cu->proto.data);
        return RPS_ERROR;
    }
    
    string_init(&up->api);
    switch (up->proto) {
    case SOCKS5:
        snprintf(api, MAX_API_LENGTH, "%s/proxy/socks5", capi->url.data);
        break;
    case HTTP:
        snprintf(api, MAX_API_LENGTH, "%s/proxy/http", capi->url.data);
        break;
    case HTTP_TUNNEL:
        snprintf(api, MAX_API_LENGTH, "%s/proxy/http_tunnel", capi->url.data);
        break;
    default:
        NOT_REACHED();
    }
    if (string_duplicate(&up->api, api, strlen(api)) != RPS_OK) {
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
    string_deinit(&up->api);
    up->timeout = 0;
    up->index = 0;
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
upstreams_init(struct upstreams *us, struct config_api *capi, 
        struct config_upstreams *cus) {

    rps_status_t status;
    rps_str_t   *schedule;
    int i, len;
    struct upstream_pool *up;
    struct config_upstream *cu;

    us->hybrid = cus->hybrid;   
    us->maxreconn = cus->maxreconn;
    us->maxretry = cus->maxretry;
    us->mr1m = cus->mr1m;
    us->mr1h = cus->mr1h;
    us->mr1d = cus->mr1d;
    us->max_fail_rate = cus->max_fail_rate;

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
        
        if (upstream_pool_init(up, cu, capi) != RPS_OK) {
            goto error;
        }
    }

    if (uv_mutex_init(&us->mutex) < 0) {
        goto error;     
    }

    if (uv_cond_init(&us->ready) < 0) {
        goto error;     
    }

    curl_global_init(CURL_GLOBAL_ALL);
    
    us->once = 0;

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

    array_deinit(&us->pools);

    uv_mutex_destroy(&us->mutex);
    uv_cond_destroy(&us->ready);
    curl_global_cleanup();
}

static rps_status_t
upstream_json_parse(struct upstream *u, json_t *element) {
    rps_str_t host;
    uint16_t port;
    void *kv;
    rps_status_t status;

    if (json_typeof(element) != JSON_OBJECT) {
        return RPS_ERROR;
    }

    port = 0;
    status = RPS_OK;
    string_init(&host);
    
    for (kv = json_object_iter(element); kv; kv = json_object_iter_next(element, kv)) {
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
        } else if (strcmp(json_object_iter_key(kv), "source") == 0) {
            /* Ignore source is null */
            if (json_typeof(tmp) == JSON_STRING) {
                status = string_duplicate(&u->source, json_string_value(tmp), json_string_length(tmp));
            }
        } else if (strcmp(json_object_iter_key(kv), "weight") == 0) {
            u->weight = (uint16_t)json_integer_value(tmp);
        } else if (strcmp(json_object_iter_key(kv), "success") == 0) {
            u->success = (uint32_t)json_integer_value(tmp);
        } else if (strcmp(json_object_iter_key(kv), "failure") == 0) {
            u->failure = (uint32_t)json_integer_value(tmp);
        } else if (strcmp(json_object_iter_key(kv), "insert_date") == 0) {
            u->insert_date = (rps_ts_t)json_integer_value(tmp);
        } else if (strcmp(json_object_iter_key(kv), "enable") == 0) {
            u->enable = (uint8_t)json_integer_value(tmp);
        } else {
            status = RPS_ERROR;
        }

        if (status != RPS_OK) {
            log_error("json parse '%s:%s' error", json_object_iter_key(kv), json_string_value(tmp));
            string_deinit(&host);
            return status;
        }
    }

    status = rps_resolve_inet((const char *)host.data, port, &u->server);
    if (status != RPS_OK) {
        log_error("jason parse error, invalid upstream address, %s:%d", host.data, port);
    }

    string_deinit(&host);
    return status;

}

static rps_status_t
upstream_pool_json_parse(rps_array_t *pool, struct curl_buf *resp) {
    json_t *root;
    json_t *element;
    json_error_t error;
    struct upstream *upstream;
    size_t  len;
    size_t i;

    i = 0;
    len = 0;

    root = json_loads((const char *)resp->buf, 0, &error);
    if (!root) {
        log_error("json decode upstream pool error: %s", error.text);
        return RPS_ERROR;
    }

    if (json_typeof(root) != JSON_ARRAY) {
        log_error("json invalid records,  response should be array");
        json_decref(root);
        return RPS_ERROR;
    }

    len = json_array_size(root);
    for (i = 0; i < len; i++) {
        element = json_array_get(root, i);
        upstream = (struct upstream *)array_push(pool);
        upstream_init(upstream);
        if (upstream_json_parse(upstream, element) != RPS_OK) {
            array_pop(pool);
            upstream_deinit(upstream);
        }
    }

    json_decref(root);

    return RPS_OK;
}

static size_t
upstream_pool_load_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize;
    struct curl_buf *resp;
        
    realsize = size * nmemb;

    resp = (struct curl_buf *)userp;
    resp->buf = rps_realloc(resp->buf, resp->len + realsize + 1);
    if (resp->buf == NULL) {
        log_error("fetech upstreams error, not enough memory");
        return 0;
    }

    memcpy(&resp->buf[resp->len], contents, realsize); 
    resp->len += realsize;
    resp->buf[resp->len] = '\0';
    
    return realsize;
}

static rps_status_t
upstream_pool_load(rps_array_t *pool, rps_str_t *api, uint32_t timeout) {
    CURL *curl_handle;
    CURLcode res;
    struct curl_buf resp;
    rps_status_t status;

    resp.buf = rps_alloc(1);
    resp.len = 0;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, api->data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, upstream_pool_load_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&resp);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "rps/curl");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, timeout);
    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK) {
        log_error("fetch upstreams from '%s' trigger error. %s", 
                api->data,  curl_easy_strerror(res));
        status = RPS_ERROR;
    } else {
        log_verb("fetch upstreams from '%s' success, %zu bytes", api->data, resp.len);
        status = RPS_OK;
    }
    
    if (status == RPS_OK) {
        upstream_pool_json_parse(pool, &resp);
    }
    
    curl_easy_cleanup(curl_handle);
    rps_free(resp.buf);

    return status;
}

static rps_status_t
upstream_pool_refresh(struct upstream_pool *up) {

    rps_array_t *new_pool;

    /* Free current upstream pool only when new pool load successful */

    new_pool = array_create(UPSTREAM_DEFAULT_POOL_LENGTH, sizeof(struct upstream));
    if (new_pool == NULL) {
        return RPS_ERROR;
    }


    if (upstream_pool_load(new_pool, &up->api, up->timeout) != RPS_OK) {
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
            return;
        } else {
            log_debug("refresh %s upstream pool, get <%d> proxys", proto, array_n(up->pool));
        }
    }
    
    //run only once
    if (us->once == 0) {
        uv_mutex_lock(&us->mutex);
        uv_cond_broadcast(&us->ready);
        uv_mutex_unlock(&us->mutex);
    }
    us->once = 1;
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

struct upstream *
upstreams_get(struct upstreams *us, rps_proto_t proto) {
    struct upstream *upstream;
    struct upstream_pool *up;
    int i, len;
    int count;
    upstream_pool_get_algorithm get_func;

    upstream = NULL;
    up = NULL;
    get_func = NULL;
    count = 0;

    if (us->hybrid) {
        if (proto == HTTP_TUNNEL || proto == SOCKS5) {
            // http_tunnel, socks5 can only forward via http_tunnel or socks5   
            for (; ;) {
                up = array_random(&us->pools);
                if (up->proto == HTTP_TUNNEL || up->proto == SOCKS5) {
                    break;
                }
            }
        } else {
            up = array_random(&us->pools);
        }
    } else {
        len = array_n(&us->pools);
        for (i=0; i<len; i++) {
            up = array_get(&us->pools, i);
            if (up->proto == proto) {
                break;
            }
        }
    }

    switch (us->schedule) {
        case up_rr:
            get_func = upstream_pool_get_rr;
            break;
        case up_random:
            get_func = upstream_pool_get_random;
            break;
        case up_wrr:
        default:
            NOT_REACHED();
    }   

    uv_rwlock_rdlock(&up->rwlock);

    for ( ; ; ) {
        if (count >= UPSTREAM_MAX_LOOP) {
            break;
        }

        upstream = get_func(up);

        count += 1;

        if (upstream == NULL) {
            break;
        }

        if (!upstream->enable) {
            continue;
        }

        if (upstream_poor_quality(upstream, us->max_fail_rate)) {
            upstream->enable = 0;
            continue;
        }

        if (upstream_fresh(upstream)) {
            upstream_init_timewheel(upstream, us->mr1m, us->mr1h, us->mr1d);
            break;
        }

        if (upstream_request_too_often(upstream, us->mr1m, us->mr1h, us->mr1d)) {
            upstream = NULL;
            continue;
        }

        break;
    }

#if RPS_DEBUG_OPEN
    if (upstream != NULL) {
        upstream_str(upstream);
    } 
#endif
    
    if (upstream != NULL) {
        upstream->count += 1;    
        if (us->mr1m > 0 || us->mr1h > 0 || us->mr1d >0) {
            upstream_timewheel_add(upstream);
        }
    }
    
    uv_rwlock_rdunlock(&up->rwlock);
    return upstream;
}
