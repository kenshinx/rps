#ifndef _RPS_S5_H
#define _RPS_S5_H


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
 * RPS work based on socks5 proxy 
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
 *                    |   +------------->  |    Connect         |                 |
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

#define S5_ERR_MAP(V)                                                       	\
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

#define SOCKS5_VERSION  5
#define SOCKS5_AUTH_PASSWD_VERSION 1

#pragma pack(push,1)

struct s5_method_request {
    uint8_t ver;
    uint8_t nmethods;
    uint8_t methods[255];
};

struct s5_method_response {
    uint8_t ver;
    uint8_t method;
};

struct s5_auth_request {
    uint8_t ver;
    uint8_t ulen;
    uint8_t uname[255];
    uint8_t plen;
    uint8_t passwd[255];
};


#pragma pack(pop)

typedef enum {
	s5_version,
	s5_nmethods,
	s5_methods,
	s5_auth_pw_version,
	s5_auth_pw_userlen,
	s5_auth_pw_username,
	s5_auth_pw_passlen,
	s5_auth_pw_password,
	s5_req_version,
	s5_req_cmd,
	s5_req_reserved,
	s5_req_atyp,
	s5_req_atyp_host,
	s5_req_daddr,
	s5_req_dport0,
	s5_req_dport1,
	s5_dead
} s5_state_t;

typedef enum {
    s5_auth_none =   0x00,
    s5_auth_gssapi = 0x01,
    s5_auth_passwd = 0x02,
    s5_auth_unacceptable = 0xFF
} s5_auth_method;

typedef enum {
    s5_auth_allow,
    s5_auth_deny
} s5_auth_result;

typedef enum {
    s5_atyp_ipv4,
    s5_atyp_ipv6,
    s5_atyp_host
} s5_atyp;

typedef enum {
    s5_cmd_tcp_connect,
    s5_cmd_tcp_bind,
    s5_cmd_udp_assoc
} s5_cmd;


typedef struct {

	uint32_t	__n;

	uint8_t		version;
	uint8_t		state;
	uint8_t		nmethods;
	uint8_t		methods[255];
	uint8_t		cmd;

} s5_handle_t;

#include "core.h"

static inline void
s5_handle_init(s5_handle_t *handle) {
	memset(handle, 0, sizeof(*handle));
	handle->state = s5_version;
}

void s5_server_do_next(struct context *ctx);
void s5_client_do_next(struct context *ctx);

#endif
