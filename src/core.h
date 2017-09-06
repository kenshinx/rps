#ifndef _RPS_CORE_H
#define _RPS_CORE_H


#include <uv.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2
#define RPS_EUPSTREAM   -3
#define RPS_EQUEUE   -4

#define READ_BUF_SIZE 2048 //2k
#define WRITE_BUF_SIZE 65536 //64k
#define WRITE_UV_BUF_SIZE   20

#define UNDEFINED_REPLY_CODE -1

#define MAX_API_LENGTH  256

#define RPS_CURL_UA "rps/curl"

typedef int rps_status_t;

//In the GNU C Library, time_t is equivalent to long int. 
typedef long int rps_ts_t;

#define RPS_PROTO_MAP(V)                      \
    V(-1, UNSUPPORT, "unsupport")             \
    V(0,  UNSET, "unset")                     \
    V(1,  SOCKS5, "socks5")                   \
    V(2,  HTTP, "http")                       \
    V(3,  HTTP_TUNNEL, "http_tunnel")         \
    V(4,  SOCKS4, "socks4")                   \
    V(5,  PRIVATE, "private")                 \

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


/* Unified reply code, will mapping with http and socks5 reponse code */
typedef enum {
    rps_rep_ok,
    rps_rep_moved_permanent,  //HTTP 301
    rps_rep_moved_temporary,  //HTTP 302
    rps_rep_not_modified,     //HTTP 304
    rps_rep_forbidden,
    rps_rep_auth_require,
    rps_rep_not_found,
    rps_rep_invalid_request,
    rps_rep_server_error,
    rps_rep_timeout,
    rps_rep_bad_request,
    rps_rep_unreachable,
    rps_rep_proxy_unavailable,
    rps_rep_undefined,
} rps_reply_code_t;

typedef struct context rps_ctx_t;
typedef struct session rps_sess_t;

#include "util.h"
#include "_string.h"
#include "log.h"
#include "array.h"
#include "hashmap.h"
#include "server.h"
#include "upstream.h"


typedef enum context_flag {
    c_request,
    c_forward
} ctx_flag_t;

typedef enum context_stream {
    c_tunnel,
    c_pipeline
} ctx_stream_t;

typedef enum context_state {
    c_init = (1 << 0),
    c_conn = (1 << 1),
    c_handshake_req = (1 << 2),
    c_handshake_resp = (1 << 3),
    c_auth_req = (1 << 4),
    c_auth_resp = (1 << 5),
    c_requests = (1 << 6),
    c_exchange = (1 << 7),
    c_reply = (1 << 8),
    c_retry = (1 << 9),
    c_failed = (1 << 10),
    c_establish = (1 << 11),
    c_established = (1 << 12),
    c_kill = (1 << 13),
    c_will_kill = (1 << 14),
    c_closing = (1 << 15),
    c_closed = (1 << 16)
} ctx_state_t;


typedef enum {
    c_busy,
    c_done,
    c_stop,
} ctx_io_state_t;

typedef void (*rps_next_t)(struct context *);

struct context {
    struct session      *sess;

    union {
        uv_handle_t     handle;
        uv_stream_t     stream;
        uv_tcp_t        tcp;
    } handle;

    rps_next_t          do_next;

    uint32_t            timeout;

    uv_timer_t          timer;
    uv_write_t          write_req;
    uv_connect_t        connect_req;
    uv_shutdown_t       shutdown_req;

    rps_proto_t         proto;

    char                rbuf[READ_BUF_SIZE];
    ssize_t             nread;

    char                *wbuf;
    ssize_t             nwrite;

    /* The memory pointed to by the buffers must remain valid until the write callback gets called.
     * So we use a buffer for write buffer ensure the wbuf is safe 
     * and won't be overwritten before write callback called.
     */
    char                *wbuf2;
    ssize_t             nwrite2;

    rps_addr_t          peer;
    char                peername[MAX_INET_ADDRSTRLEN];

    ctx_flag_t          flag;
    ctx_stream_t        stream;
    ctx_state_t         state;


    /* HTTP proxy and HTTP tunnel proxy need this pointer to transmit request params 
     * from client to upstream (method, url, headers, e.g.) 
     */
    void                *req;

    /* reply code is protocol related
     * http tunnel may be http_ok or http_forbidden etc.. 
     * socks5 may be s5_rep_success, s5_rep_conn_deny etc..
     */
    int                 reply_code;

    int                 last_status;
    uint16_t            reconn;
    uint16_t            retry;

    /* closing count, free context memory while counter value be 2
     * 2 means both timer and connect have been closed.
     * */
    uint8_t             c_count;

    uint8_t             rstat;
    uint8_t             wstat;

    uint8_t             connecting:1;
    uint8_t             connected:1;
    uint8_t             established:1;
};

struct session {
    struct server   *server;

    struct context  *request;
    struct context  *forward;

    struct upstream *upstream;

    struct timeval  start;
    struct timeval  end; 

    rps_addr_t remote;
};

#endif
