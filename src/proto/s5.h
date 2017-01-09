#ifndef _RPS_S5_H
#define _RPS_S5_H

#include "server.h"

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
 *      S5 HandShake  |     S5_Method      |                    |                 |
 *   +--------------+ |  <--------------+  |                    |                 |
 *                    |                    |                    |                 |
 *                    |                    |                    |                 |
 *                    |    S5_Auth         |                    |                 |
 *                    |   +------------->  |                    |                 |
 *                    |                    |                    |                 |
 *    S5 SubNegotiate |    S5_Auth_result  |                    |                 |
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
 * RPS Handshake sucess                    |                    |                 |
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
	S5_VERSION,

} s5_phase_t;

void s5_do_next(struct context *ctx, const char *data, ssize_t nread);

#endif
