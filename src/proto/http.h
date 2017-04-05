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

#define HTTP_HEADER_MAX_KEY_LENGTH     256
#define HTTP_HEADER_MAX_VALUE_LENGTH   512

//http body size always be small in our approach.
#define HTTP_BODY_MAX_LENGTH    256
// 1k is big enough in our approach
#define HTTP_MESSAGE_MAX_LENGTH    1024

static const char HTTP_DEFAULT_PROTOCOL[] = "HTTP/1.1";
static const char HTTP_DEFAULT_AUTH[] = "Basic";
static const char HTTP_DEFAULT_REALM[] = "rps";
static const char HTTP_DEFAULT_PROXY_AGENT[] = "RPS/1.0";


#define HTTP_RESP_MAP(V)                                                \
    V(0,   http_undefine, "Undefine")                                   \
    V(200, http_ok, "OK")                                               \
    V(403, http_forbidden, "Forbidden")                                 \
    V(407, http_proxy_auth_required, "Proxy Authentication Required")   \
    V(500, http_server_error, "Internal Server Error")                  \
    V(502, http_bad_gateway, "Bad Gateway")                             \

enum {
#define HTTP_RESP_GEN(code, name, _) name = code,
    HTTP_RESP_MAP(HTTP_RESP_GEN)
#undef HTTP_RESP_GEN
};


static inline const char * 
http_resp_code_str(uint16_t code) {
#define HTTP_RESP_GEN(_, name, str) case name: return str;
    switch (code) {
        HTTP_RESP_MAP(HTTP_RESP_GEN)
        default: ;
    }
#undef HTTP_RESP_GEN
    return "Invalid response status code.";
}

#define HTTP_METHOD_MAP(V)                  \
    V(0, http_emethod, "EMETHOD")           \
    V(1, http_get, "GET")                   \
    V(2, http_post, "POST")                 \
    V(3, http_connect, "CONNECT")           \

enum {
#define HTTP_METHOD_GEN(code, name, _) name = code,
    HTTP_METHOD_MAP(HTTP_METHOD_GEN)
#undef HTTP_METHOD_GEN
};

static inline const char * 
http_method_str(uint16_t code) {
#define HTTP_METHOD_GEN(_, name, str) case name: return str;
    switch (code) {
        HTTP_METHOD_MAP(HTTP_METHOD_GEN)
        default: ;
    }
#undef HTTP_METHOD_GEN
    return "EMETHOD";
}

enum http_request_verify_result {
    http_verify_error = -1,
    http_verify_success = 0,
    http_verify_fail,
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
void http_response_init(struct http_response *resp);
void http_response_deinit(struct http_response *resp);


rps_status_t http_request_parse(struct http_request *req, uint8_t *data, size_t size);
rps_status_t http_request_auth_parse(struct http_request_auth *auth, 
    uint8_t *credentials, size_t credentials_size);

int http_basic_auth(struct context *ctx, rps_str_t *param);
int http_basic_auth_gen(const char *uname, const char *passwd, char *output);

#ifdef RPS_DEBUG_OPEN
void http_request_dump(struct http_request *req);
void http_response_dump(struct http_response *resp);
#endif

int http_response_message(char *message, struct http_response *resp);

#endif
