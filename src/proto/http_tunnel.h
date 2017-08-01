#ifndef _RPS_HTTP_TUNNEL_H
#define _RPS_HTTP_TUNNEL_H

#include "http.h"

/*
 * Related RFC:
 *
 * Tunneling TCP based protocols through Web proxy servers
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 *
 * HTTP Authentication: Basic and Digest Access Authentication
 * https://tools.ietf.org/html/rfc2617
 * 
 *
 * RPS http tunnel proxy establishment procedure
 * 
 *             Client                   RPS                 Upstream        Remote
 *
 *              +                        
 *  HandShake   |  HTTP Connect          +                       +              +
 *  +--------+  | ------------------->   |                       |              |
 *              |  Host:                 |                       |              |
 *              |  Proxy_Authorization:  |                       |              |
 *              |  (Maybe)               |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *Handshake_resp|  HTTP 407 Auth Require |                       |              |
 * +-----------+| <--------------------  |                       |              |
 *              | Porxy_Authenticate     |                       |              |
 *              |                        |                       |              |
 * Authenticate |  HTTP Connect          |                       |              |
 * +-----------+| ---------------------> |                       |              |
 *              |  Host:                 |                       |              |
 *              |  Proxy_Authorization:  |                       |              |
 *              |                        |  TCP Connect          |              |
 *              |                        | ------------------>   |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |  HTTP Connect         |              |
 *              |                        |  +---------------->   |              |
 *              |                        |  Host:                |              |
 *              |                        |  Proxy_Authorization: |              |
 *              |                        |  (Maybe)              |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        | HTTP 407 Auth Require |              |
 *              |                        | <-------------------- |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |  HTTP Connect         |              |
 *              |                        |  -------------------> |              |
 *              |                        |  Host:                | TCP Connect  |
 *              |                        |  Proxy_Authorization: | +----------> |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |   HTTP 200 OK         |              |
 *              |                        | <-----------------+   |              |
 *              |                        |                       |              |
 *  Established |    HTTP 200 OK                                 |              |
 * +----------+ | <--------------------+ |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |   TCP Payload          |    Traffic Forward    |  TCP Payload |
 *              |  +------------------>  |  +--------------->    | +----------> |
 *              |   HTTP(S),WHOIS,ETC    |                       |              |
 *              |                        |                       |              |
 *              |       Response         |     Response          |   Response   |
 *              |     <-------------+    |     <-----------+     |  <--------+  |
 *              |                        |                       |              |
 *              +                        +                       +              +
 */


void http_tunnel_server_do_next(struct context *ctx);
void http_tunnel_client_do_next(struct context *ctx);


#endif
