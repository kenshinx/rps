#ifndef _RPS_CORE_H
#define _RPS_CORE_H


#include <uv.h>
#include <string.h>
#include <stdint.h>

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2
#define RPS_EUPSTREAM   -3

#define	READ_BUF_SIZE 2048 //2k

typedef int rps_status_t;

#define RPS_PROTO_MAP(V)                      \
    V(-1, UNSUPPORT, "unsupport")             \
    V(1,  SOCKS5, "socks5")                   \
    V(2,  HTTP, "http")                       \
    V(3,  SOCKS4, "socks4")                   \
    V(4,  PRIVATE, "private")                 \

typedef enum {
#define RPS_PROTO_GEN(code, name, _) name = code,
      RPS_PROTO_MAP(RPS_PROTO_GEN)
#undef RPS_PROTO_GEN
} rps_proto_t;

static inline const char * 
rps_proto_str(rps_proto_t proto) {
#define RPS_PROTO_GEN(_, name, str) case name: return str;
    switch (proto) {
        RPS_PROTO_MAP(RPS_PROTO_GEN)
        default: ;
    }
#undef RPS_PROTO_GEN
    return "Unsupport Proto";
}

static inline rps_proto_t
rps_proto_int(const char *proto) {
#define RPS_PROTO_GEN(_, name, str) if (strcmp(proto, str) == 0) {return name;}
    RPS_PROTO_MAP(RPS_PROTO_GEN)
#undef RPS_PROTO_GEN
    return UNSUPPORT;
}


typedef struct context rps_ctx_t;
typedef struct session rps_sess_t;

#include "util.h"
#include "_string.h"
#include "log.h"
#include "array.h"
#include "server.h"
#include "upstream.h"



typedef enum context_flag {
    c_request,
    c_forward
} ctx_flag_t;

typedef enum context_state {
    c_init = (1 << 0),
    c_conn = (1 << 1),
	c_handshake = (1 << 2),
    c_handshake_reply = (1 << 3),
	c_auth = (1 << 4),
    c_auth_reply = (1 << 5),
    c_requests = (1 << 6),
    c_exchange = (1 << 7),
    c_reply = (1 << 8),
	c_established = (1 << 9),
    c_kill = (1 << 10),
    c_dead = (1 << 11),
    c_closing = (1 << 12),
    c_closed = (1 << 13)
} ctx_state_t;


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
    uv_connect_t        connect_req;

    rps_proto_t     	proto;

	char 				buf[READ_BUF_SIZE];
	ssize_t				nread;

    rps_addr_t          peer;
    char            	peername[MAX_INET_ADDRSTRLEN];

    ctx_flag_t         	flag;
    ctx_state_t        	state;

    int                 last_status;
    uint32_t            retry;

    uint8_t             connected:1;
};

struct session {
    struct server  *server;

    struct context *request;
    struct context *forward;

    struct upstream  upstream;

    rps_addr_t client;
    rps_addr_t remote;
};

#endif
