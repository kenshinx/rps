#ifndef _RPS_S5_H
#define _RPS_S5_H

#include "server.h"

#include <uv.h>

/*
 * RPS work based on socks5 proxy 
 * https://www.ietf.org/rfc/rfc1928.txt
 *
 *                  Client                RPS               Upstream            Remote
 *
 *                   |                    |                    |                 |
 *                   |     S5_Version     |                    |                 |
 *                   |  +------------->   |                    |                 |
 *                   |                    |                    |                 |
 *     S5 HandShake  |     S5_Method      |                    |                 |
 *  +--------------+ |  <--------------+  |                    |                 |
 *                   |                    |                    |                 |
 *                   |                    |                    |                 |
 *                   |    S5_Auth         |                    |                 |
 *                   |   +------------->  |                    |                 |
 *                   |                    |                    |                 |
 *   S5 SubNegotiate |    S5_Auth_result  |                    |                 |
 *  +--------------+ |   <--------------+ |                    |                 |
 *                   |                    |                    |                 |
 *                   |                    |                    |                 |
 *                   |    S5_Request      |                    |                 |
 *                   |   +------------->  |    Connect         |                 |
 *                   |                    |  +------------->   |                 |
 *                   |                    |                    |                 |
 *                   |                    |     S5_Version     |                 |
 *                   |                    |  +------------->   |                 |
 *                   |                    |                    |                 |
 *                   |      S5 HandShake  |     S5_Method      |                 |
 *                   |    +-------------+ |  <--------------+  |                 |
 *                   |                    |                    |                 |
 *                   |                    |                    |                 |
 *                   |                    |    S5_Auth         |                 |
 *                   |                    |   +------------->  |                 |
 *                   |                    |                    |                 |
 *                   |    S5 SubNegotiate |    S5_Auth_result  |                 |
 *                   |   +--------------+ |   <--------------- |                 |
 *                   |                    |                    |                 |
 *                   |                    |                    |                 |
 *                   |                    |    S5_Request      |                 |
 *                   |                    |   +------------->  |   Connect       |
 *                   |                    |                    | +-------------> |
 *                   |                    |    S5_Reply        |                 |
 *                   |                    |   <-------------+  |                 |
 *                   |                    |                    |                 |
 *                   |    S5_Reply        |                    |                 |
 *                   |   <-------------+  |                    |                 |
 *RPS Handshake sucess                    |                    |                 |
 *+------------------|                    |                    |                 |
 *                   |                    |                    |                 | */

void s5_do_parse(struct context *ctx, const char *data, ssize_t nread);

#endif
