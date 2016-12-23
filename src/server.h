#ifndef _RPS_SERVER_H
#define _RPS_SERVER_H

#include "core.h"
#include "string.h"
#include "config.h"

#include <uv.h>

#include <unistd.h>

#include "util.h"
#include "core.h"


#define HTTP_DEFAULT_BACKLOG  65536
#define TCP_KEEPALIVE_DELAY 120

struct server {
    uv_loop_t               loop;   
    uv_tcp_t                us; /* libuv tcp server */

    rps_proxy_t             proxy;
    
    struct rps_addr_t       listen;
    
    struct config_server   *cfg;
};

rps_status_t server_init(struct server *s, struct config_server *cs);
void server_deinit(struct server *s);
void server_run(struct server *s);

/*
 * server_stop
 */


typedef struct session {
    struct context  *ctx;
    uv_tcp_t        handler;
    uv_timer_t      timer;
    uv_write_t      write_req;
} rps_sess_t;

typedef struct context {
    /*
    rps_addr_t client;
    rps_addr_t upstream;
    rps_addr_t remote;
    */

    rps_sess_t request;
    rps_sess_t forward;
} rps_ctx_t;

#endif
