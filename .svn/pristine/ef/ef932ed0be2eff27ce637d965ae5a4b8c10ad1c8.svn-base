#ifndef _RPS_SERVER_H
#define _RPS_SERVER_H

#include "core.h"
#include "config.h"
#include "util.h"
#include "_string.h"
#include "upstream.h"

#include <uv.h>

#include <unistd.h>

#define TCP_BACKLOG  65536
#define TCP_KEEPALIVE_DELAY 120


struct server {
    uv_loop_t               loop;   
    uv_tcp_t                us; /* libuv tcp server */

    rps_proto_t             proto;
    
    rps_addr_t              listen;
    
    uint32_t                rtimeout; /* request context timeout */
    uint32_t                ftimeout; /* forward context timeout */

    struct config_server    *cfg;

    struct upstreams        *upstreams;
};

rps_status_t server_init(struct server *s, struct config_server *cs, 
        struct upstreams *us, uint32_t rtimeout, uint32_t ftimeout);
void server_deinit(struct server *s);
void server_run(struct server *s);
// void server_stop(struct server *);

void server_do_next(rps_ctx_t *ctx);

rps_status_t server_write(struct context *ctx, const void *data, size_t len);

#endif
