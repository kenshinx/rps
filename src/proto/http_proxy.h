#ifndef _RPS_HTTP_PROXY_H
#define _RPS_HTTP_PROXY_H

#include "http.h"


/*
 * Related RFC:
 *
 * Hypertext Transfer Protocol -- HTTP/1.1
 * https://tools.ietf.org/html/rfc2616
 *
 * HTTP Authentication: Basic and Digest Access Authentication
 * https://tools.ietf.org/html/rfc2617
 * 
 *
 * RPS http proxy establishment procedure
 * 
 *             Client                   RPS                 Upstream        Remote
 *
 *              +                        +                       +              +
 *   Request    |    HTTP Request        |                       |              |
 *  +--------+  | +------------------>   |                       |              |
 *              |  Host:                 |                       |              |
 *              |  Proxy_Authorization:  |                       |              |
 *              |  (Maybe)               |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *  Response    |  HTTP 407 Auth Require |                       |              |
 * +------------+ <-------------------+  |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 * Authenticate |  HTTP Request          |                       |              |
 * +------------+ +--------------------> |                       |              |
 *              |  Host:                 |                       |              |
 *              |  Proxy_Authorization:  |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |  HTTP Request         |              |
 *              |                        |  +---------------->   |              |
 *              |                        |  Host:                |              |
 *              |                        |  Proxy_Authorization: |              |
 *              |                        |  (Maybe)              |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        | HTTP 407 Auth Require |              |
 *              |                        | <-------------------+ |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |  HTTP Request         |              |
 *              |                        |  +------------------> |              |
 *              |                        |  Host:                | HTTP Request |
 *              |                        |  Proxy_Authorization: | +----------> |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       | HTTP Response|
 *              |                        |     HTTP Response     | <----------+ |
 *  Finished    |    HTTP Response       |  <-----------------+  |              |
 * +----------+ | <-------------------+  |                       |              |
 *              |                        |                       |              |
 *              +                        +                       +              +
*/

void http_proxy_server_do_next(struct context *ctx);
void http_proxy_client_do_next(struct context *ctx);

#endif
