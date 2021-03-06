#ifndef _RPS_S5_H
#define _RPS_S5_H

#include "core.h"

#include <uv.h>


/* SOCKS5 Protocol
 *
 * Related RFC:
 * 
 * SOCKS Protocol Version 5
 * https://www.ietf.org/rfc/rfc1928.txt
 *
 * Username/Password Authentication for SOCKS V5
 * https://tools.ietf.org/html/rfc1929
 *
 * A SOCKS-based IPv6/IPv4 Gateway Mechanism
 * https://tools.ietf.org/html/rfc3089
 *
 * RPS socks5 proxy tunnel establishment procedure
 *
 *                   Client                RPS               Upstream           Remote
 * 
 *                    |                    |                    |                 |
 *                    |     S5_Version     |                    |                 |
 *                    |  +------------->   |                    |                 |
 *                    |                    |                    |                 |
 *         HandShake  |     S5_Method      |                    |                 |
 *   +--------------+ |  <--------------+  |                    |                 |
 *                    |                    |                    |                 |
 *                    |                    |                    |                 |
 *                    |    S5_Auth         |                    |                 |
 *                    |   +------------->  |                    |                 |
 *                    |                    |                    |                 |
 *       SubNegotiate |    S5_Auth_result  |                    |                 |
 *   +--------------+ |   <--------------+ |                    |                 |
 *                    |                    |                    |                 |
 *                    |                    |                    |                 |
 *                    |    S5_Request      |                    |                 |
 *                    |   +------------->  |    TCP Connect     |                 |
 *                    |                    |  +------------->   |                 |
 *                    |                    |                    |                 |
 *                    |                    |     S5_Version     |                 |
 *                    |                    |  +------------->   |                 |
 *                    |                    |                    |                 |
 *                    |      S5 HandShake  |     S5_Method      |                 |
 *                    |    +-------------+ |  <--------------+  |                 |
 *                    |                    |                    |                 |
 *                    |                    |                    |                 |
 *                    |                    |    S5_Auth         |                 |
 *                    |                    |   +------------->  |                 |
 *                    |                    |                    |                 |
 *                    |    S5 SubNegotiate |    S5_Auth_result  |                 |
 *                    |   +--------------+ |   <--------------- |                 |
 *                    |                    |                    |                 |
 *                    |                    |                    |                 |
 *                    |                    |    S5_Request      |                 |
 *                    |                    |   +------------->  |   Connect       |
 *                    |                    |                    | +-------------> |
 *                    |                    |    S5_Reply        |                 |
 *                    |                    |   <-------------+  |                 |
 *                    |                    |                    |                 |
 *                    |    S5_Reply        |                    |                 |
 *                    |   <-------------+  |                    |                 |
 *       Established  |                    |                    |                 |
 * +------------------|                    |                    |                 |
 *                    |                    |                    |                 |
 */

#define S5_ERR_MAP(V)                                                           \
    V(-1, s5_bad_version, "Bad protocol version.")                              \
    V(-2, s5_bad_cmd, "Bad protocol command.")                                  \
    V(-3, s5_bad_atyp, "Bad address type.")                                     \
    V(-4, s5_auth_error, "Auth failure.")                                       \
    V(-5, s5_need_more_data, "Need more data.")                                 \
    V(0,  s5_ok, "No error.")                                                   \
    V(1,  s5_auth_select, "Select authentication method.")                      \
    V(2,  s5_auth_verify, "Verify authentication.")                             \
    V(3,  s5_exec_cmd, "Execute command.")                                      \

typedef enum {
#define S5_ERR_GEN(code, name, _) name = code,
      S5_ERR_MAP(S5_ERR_GEN)
#undef S5_ERR_GEN
} s5_err_t;

static inline const char * 
s5_strerr(s5_err_t err) {
#define S5_ERR_GEN(_, name, errmsg) case name: return errmsg;
    switch (err) {
        S5_ERR_MAP(S5_ERR_GEN)
        default: ;
    }
#undef S5_ERR_GEN
    return "Unknown error.";
}

#define S5_REP_MAP(V)                                               \
    V(0x00, s5_rep_success, "success")                              \
    V(0x01, s5_rep_socks_fail, "socks failure")                     \
    V(0x02, s5_rep_conn_deny, "connect denied")                     \
    V(0x03, s5_rep_net_unreach, "network unreachable")              \
    V(0x04, s5_rep_host_unreach, "host unreachable")                \
    V(0x05, s5_rep_conn_refuse, "connect refused")                  \
    V(0x06, s5_rep_ttl_expire, "ttl expired")                       \
    V(0x07, s5_rep_cmd_not_support, "cmd not support")              \
    V(0x08, s5_rep_addr_not_support, "address type not support")    \
    V(0x09, s5_rep_unassigned, "unassigned")                        \
    V(0x0a, s5_rep_proxy_unavailable, "s5_rep_proxy_unavailable")   \

typedef enum {
#define S5_REP_GEN(code, name, _) name = code,
      S5_REP_MAP(S5_REP_GEN)
#undef S5_REP_GEN
} s5_rep_t;

static inline const char * 
s5_strrep(s5_rep_t code) {
#define S5_REP_GEN(_, name, str) case name: return str;
    switch (code) {
        S5_REP_MAP(S5_REP_GEN)
        default: ;
    }
#undef S5_REP_GEN
    return "Unknown rep.";
}

#define S5_REPLY_CODE_MAP(V)                                    \
    V(s5_rep_success,           rps_rep_ok)                     \
    V(s5_rep_socks_fail,        rps_rep_bad_request)            \
    V(s5_rep_conn_deny,         rps_rep_auth_require)           \
    V(s5_rep_net_unreach,       rps_rep_unreachable)            \
    V(s5_rep_conn_refuse,       rps_rep_forbidden)              \
    V(s5_rep_ttl_expire,        rps_rep_timeout)                \
    V(s5_rep_cmd_not_support,   rps_rep_invalid_request)        \
    V(s5_rep_proxy_unavailable, rps_rep_proxy_unavailable)      \
    V(s5_rep_unassigned,        rps_rep_undefined)              \

static inline int
s5_reply_code_lookup(int code) {
    #define S5_REPLY_CODE_GEN(c1, c2) case c1: return c2;
    switch(code) {
        S5_REPLY_CODE_MAP(S5_REPLY_CODE_GEN)
        default: ;
    }
    #undef S5_REPLY_CODE_GEN
    return rps_rep_undefined;
}

static inline int
s5_reply_code_reverse(int code) {
    #define S5_REPLY_CODE_GEN(c1, c2) case c2: return c1;
    switch(code) {
        S5_REPLY_CODE_MAP(S5_REPLY_CODE_GEN)
        default: ;
    }
    #undef S5_REPLY_CODE_GEN
    return s5_rep_unassigned;
}


#define SOCKS5_VERSION  5
#define SOCKS5_AUTH_PASSWD_VERSION 1

#define MAX_HOSTNAME_LEN   255

#pragma pack(push,1)

/*
 *  +----+----------+----------+
 *  |VER | NMETHODS | METHODS  |
 *  +----+----------+----------+
 *  | 1  |    1     | 1 to 255 |
 *  +----+----------+----------+
 */
struct s5_method_request {
    uint8_t ver;
    uint8_t nmethods;
    uint8_t methods[255];
};

/*
 *  +----+--------+
 *   |VER | METHOD |
 *   +----+--------+
 *   | 1  |   1    |
 *   +----+--------+
 */
struct s5_method_response {
    uint8_t ver;
    uint8_t method;
};

/*
 *  +----+------+----------+------+----------+
 *  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
 *  +----+------+----------+------+----------+
 *  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
 *  +----+------+----------+------+----------+
 */
struct s5_auth_request {
    uint8_t ver;
    uint8_t ulen;
    uint8_t uname[255];
    uint8_t plen;
    uint8_t passwd[255];
};

/*
 *  +----+--------+
 *  |VER | STATUS |
 *  +----+--------+
 *  | 1  |   1    |
 *  +----+--------+
 */
struct s5_auth_response {
    uint8_t ver;
    uint8_t status;
};

/*
 *  +----+-----+-------+------+----------+----------+
 *  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 *  +----+-----+-------+------+----------+----------+
 *  | 1  |  1  | X'00' |  1   | Variable |    2     |
 *  +----+-----+-------+------+----------+----------+
 */
struct s5_request {
    uint8_t ver;
    uint8_t cmd;
    uint8_t rsv;
    uint8_t atyp;
    uint8_t daddr[MAX_HOSTNAME_LEN]; /* Max hostname length */
    uint8_t dport[2];
};

/*
 * +----+-----+-------+------+----------+----------+
 *  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 *  +----+-----+-------+------+----------+----------+
 *  | 1  |  1  | X'00' |  1   | Variable |    2     |
 *  +----+-----+-------+------+----------+----------+
 */

#define S5_RESPONSE_FIELDS  \
    uint8_t ver;            \
    uint8_t rep;            \
    uint8_t rsv;            \
    uint8_t atyp;

struct s5_in4_response {
    S5_RESPONSE_FIELDS
    uint8_t baddr[4];
    uint8_t bport[2];
};

struct s5_in6_response {
    S5_RESPONSE_FIELDS
    uint8_t baddr[16];
    uint8_t bport[2];
};

#undef S5_RESPONSE_FIELDS
#pragma pack(pop)


static inline void
s5_in4_response_init(struct s5_in4_response *resp) {
    memset(resp, 0, sizeof(struct s5_in4_response));
    resp->ver = 0x05;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
}

typedef enum {
    s5_auth_none =          0x00,
    s5_auth_gssapi =        0x01,
    s5_auth_passwd =        0x02,
    s5_auth_unacceptable =  0xFF
} s5_auth_method;

typedef enum {
    s5_auth_allow,
    s5_auth_deny
} s5_auth_result;

typedef enum {
    s5_atyp_ipv4 =   0x01,
    s5_atyp_domain = 0x03,
    s5_atyp_ipv6 =   0x04
} s5_atyp;

typedef enum {
    s5_cmd_tcp_connect = 0x01,
    s5_cmd_tcp_bind    = 0x02,
    s5_cmd_udp_assoc   = 0x03
} s5_cmd;

void s5_server_do_next(struct context *ctx);
void s5_client_do_next(struct context *ctx);

#endif
