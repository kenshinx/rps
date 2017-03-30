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
    rps_str_t           version;
    uint16_t            code;
    rps_hashmap_t       headers;        

    rps_str_t           body;
};


void http_server_do_next(struct context *ctx);
void http_client_do_next(struct context *ctx);

/* Only be used in http moudle internal */

void http_request_init(struct http_request *req);
void http_request_deinit(struct http_request *req);
void http_request_auth_init(struct http_request_auth *auth);
void http_request_auth_deinit(struct http_request_auth *auth);
size_t http_read_line(uint8_t *data, size_t start, size_t end, rps_str_t *line);
rps_status_t http_parse_request_line(rps_str_t *line, struct http_request *req);
rps_status_t http_parse_header_line(rps_str_t *line, rps_hashmap_t *headers);
rps_status_t http_parse_request_auth(struct http_request_auth *auth, 
    uint8_t *credentials, size_t credentials_size);

rps_status_t http_request_parse(struct http_request *req, uint8_t *data, size_t size);

int http_basic_auth(struct context *ctx, rps_str_t *param);

rps_status_t http_request_check(struct http_request *req);
#ifdef RPS_DEBUG_OPEN
void http_header_dump(void *key, size_t key_size, void *value, size_t value_size);
void http_request_dump(struct http_request *req);
#endif


#endif
