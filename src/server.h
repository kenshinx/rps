#ifndef _RPS_SERVER_H
#define _RPS_SERVER_H

#include "core.h"
#include "string.h"
#include "config.h"
#include "util.h"

#include <uv.h>

#include <unistd.h>

#define HTTP_DEFAULT_BACKLOG  65536
#define TCP_KEEPALIVE_DELAY 120

#define REQUEST_CONTEXT_TIMEOUT 30000 // 30 seconds
#define FORWARD_CONTEXT_TIMEOUT 30000 // 30 seconds

struct server {
    uv_loop_t               loop;   
    uv_tcp_t                us; /* libuv tcp server */

    rps_proxy_t             proxy;
    
    rps_addr_t             listen;
    
    struct config_server    *cfg;
};

rps_status_t server_init(struct server *s, struct config_server *cs);
void server_deinit(struct server *s);
void server_run(struct server *s);

/*
 * server_stop
 */


typedef struct context {
    struct session  *sess;

    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t    tcp;
    } handle;

    uv_timer_t      timer;
    uv_write_t      write_req;
} rps_ctx_t;

typedef struct session {
    rps_addr_t client;
    rps_addr_t upstream;
    rps_addr_t remote;

    rps_ctx_t request;
    rps_ctx_t forward;
} rps_sess_t;

#endif
