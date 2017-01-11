#ifndef _RPS_CORE_H
#define _RPS_CORE_H

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2

#include "util.h"
#include "log.h"
#include "proto/s5.h"
#include "proto/http.h"

#include <uv.h>

#include <stdint.h>

#define	READ_BUF_LENGTH 2048 //2k


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

struct context {
    struct session 		*sess;

    union {
        uv_handle_t 	handle;
        uv_stream_t 	stream;
        uv_tcp_t    	tcp;
    } handle;

	union {
		s5_handle_t		s5;
		http_handle_t	http;
#ifdef SOCKS4_PROXY_SUPPORT
		s4_handle_t		s4;
#endif
	} proxy_handle;

	s5_next_t			s5_do_next;
	http_next_t			http_do_next;
#ifdef SOCKS4_PROXY_SUPPORT
	s4_next_t			s4_do_next;
#endif
	
	

    uv_timer_t      	timer;
    uv_write_t      	write_req;

    rps_proxy_t     	proxy;

	char 				buf[READ_BUF_LENGTH];
	ssize_t				nread;

    char            	peername[MAX_INET_ADDRSTRLEN];

    uint8_t         	flag;
    uint16_t        	state;
};

struct session {
    rps_addr_t client;
    rps_addr_t upstream;
    rps_addr_t remote;

    struct context *request;
    struct context *forward;
};

#endif
