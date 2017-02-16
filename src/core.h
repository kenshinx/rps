#ifndef _RPS_CORE_H
#define _RPS_CORE_H

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2

#define	READ_BUF_SIZE 2048 //2k

typedef int rps_status_t;

typedef enum {
    SOCKS5,
    HTTP,

#ifdef SOCKS4_PROXY_SUPPORT
    SOCKS4,
#endif

#ifdef PRIVATE_PROXY_SUPPORT
    PRIVATE,
#endif

} rps_proxy_t;

typedef struct context rps_ctx_t;
typedef struct session rps_sess_t;

#include "util.h"
#include "_string.h"
#include "log.h"
#include "array.h"
#include "server.h"

#include <uv.h>

#include <stdint.h>


enum context_flag {
    c_request,
    c_forward
};

enum context_state {
    c_init = (1 << 0),
	c_handshake = (1 << 1),
	c_auth = (1 << 2),
    c_requests = (1 << 3),
	c_established = (1 << 4),
    c_kill = (1 << 5),
    c_dead = (1 << 6),
    c_closing = (1 << 7),
    c_closed = (1 << 8)
};


typedef void (*rps_next_t)(struct context *);


struct context {
    struct session 		*sess;

    union {
        uv_handle_t 	handle;
        uv_stream_t 	stream;
        uv_tcp_t    	tcp;
    } handle;

	rps_next_t			do_next;
	

    uv_timer_t      	timer;
    uv_write_t      	write_req;

    rps_proxy_t     	proxy;

	char 				buf[READ_BUF_SIZE];
	ssize_t				nread;

    char            	peername[MAX_INET_ADDRSTRLEN];

    uint8_t         	flag;
    uint16_t        	state;

    int                 last_status;
};

struct session {
    rps_addr_t client;
    rps_addr_t upstream;
    rps_addr_t remote;

    struct server  *server;

    struct context *request;
    struct context *forward;
};

#endif
