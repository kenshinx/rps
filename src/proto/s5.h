#ifndef _RPS_S5_H
#define _RPS_S5_H


#include <uv.h>


/*
 * RPS work based on socks5 proxy 
 * https://www.ietf.org/rfc/rfc1928.txt
 *
 *                   Client                RPS               Upstream            Remote
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
    V(-1, S5_BAD_VERSION, "Bad protocol version.")                              \
    V(-2, S5_BAD_CMD, "Bad protocol command.")                                  \
    V(-3, S5_BAD_ATYP, "Bad address type.")                                     \
    V(-4, S5_AUTH_ERROR, "Auth failure.")                                       \
    V(-5, S5_NEED_MORE_DATA, "Need more data.")                                 \
    V(0,  S5_OK, "No error.")                                                   \
    V(1,  S5_AUTH_SELECT, "Select authentication method.")                      \
    V(2,  S5_AUTH_VERIFY, "Verify authentication.")                             \
    V(3,  S5_EXEC_CMD, "Execute command.")                                      \

typedef enum {
#define S5_ERR_GEN(code, name, _) name = code,
      S5_ERR_MAP(S5_ERR_GEN)
#undef S5_ERR_GEN
} s5_err_t;

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

typedef struct {

	uint32_t	arg0;
	uint32_t	arg1;

	uint8_t		state;
	uint8_t		methods;
	uint8_t		cmd;

} s5_handle_t;

#include "core.h"

void s5_server_do_next(struct context *ctx);
void s5_client_do_next(struct context *ctx);

#endif
