#ifndef _RPS_SERVER_H
#define _RPS_SERVER_H

#include "core.h"
#include "config.h"
#include "util.h"
#include "_string.h"

#include <uv.h>

#include <unistd.h>

#define HTTP_DEFAULT_BACKLOG  65536
#define TCP_KEEPALIVE_DELAY 120

#define REQUEST_CONTEXT_TIMEOUT 30000 // 30 seconds
#define FORWARD_CONTEXT_TIMEOUT 30000 // 30 seconds

#define	READ_BUF_LENGTH 2048 //2k

struct server {
    uv_loop_t               loop;   
    uv_tcp_t                us; /* libuv tcp server */

    rps_proxy_t             proxy;
    
    rps_addr_t              listen;
    
    struct config_server    *cfg;
};

rps_status_t server_init(struct server *s, struct config_server *cs);
void server_deinit(struct server *s);
void server_run(struct server *s);

/*
 * server_stop
 */

enum context_flag {
    c_request,
    c_forward
};

enum context_state {
    c_init = (1 << 1),
	c_handshake = (1 << 2),
	c_auth = (1 << 3),
	c_established = (1 << 4),
    c_closing = (1 << 5),
    c_closed = (1 << 6)
};


typedef struct context rps_ctx_t;
typedef struct session rps_sess_t;

typedef uint16_t (* rps_next_t)(struct context *);

struct context {
    struct session  *sess;

    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t    tcp;
    } handle;

    uv_timer_t      timer;
    uv_write_t      write_req;

    rps_proxy_t     proxy;

	char 			buf[READ_BUF_LENGTH];
	ssize_t			nread;

    rps_next_t      do_handshake;
	rps_next_t		do_auth;

    char            peername[MAX_INET_ADDRSTRLEN];

    uint8_t         flag;
    uint16_t        state;
};

struct session {
    rps_addr_t client;
    rps_addr_t upstream;
    rps_addr_t remote;

    struct context *request;
    struct context *forward;
};

#endif
