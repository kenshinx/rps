#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "core.h"

#include <uv.h>

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
 * RPS http proxy tunnel establishment procedure
 * 
 *             Client                   RPS                 Upstream        Remote
 *
 *              +                        
 *              |  HTTP Connect          +                       +              +
 *              | ------------------->   |                       |              |
 *              |  Host:                 |                       |              |
 *   HandShake  |  Proxy_Authorization:  |                       |              |
 *   +--------+ |  (Maybe)               |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |                        |                       |              |
 *              |  HTTP 407 Auth Require |                       |              |
 *              | <--------------------  |                       |              |
 *              |                        |                       |              |
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

#define HTTP_HEADER_DEFAULT_COUNT   64
#define HTTP_HEADER_REHASH_THRESHOLD   0.05

enum http_method {
    http_emethod = 0,
    http_get = 1,
    http_post,
    http_connect,
};

enum http_staut_code {
    http_success = 200,
    http_forbidden = 403,
    http_proxy_auth_required = 407,
    http_bad_gateway = 502
};

enum http_auth_schema {
    http_auth_unknown = 0,
    http_auth_basic = 1,
    http_auth_digest,
};

struct http_request_auth {
    uint8_t             schema;
    rps_str_t           param;
};

struct http_request {
    uint8_t             method;
    rps_str_t           host;
    int                 port;
    rps_str_t           protocol;
    rps_hashmap_t       headers;        
    
};

struct http_response {
    rps_str_t   version;
    uint16_t    status;
};


void http_server_do_next(struct context *ctx);
void http_client_do_next(struct context *ctx);

#endif
