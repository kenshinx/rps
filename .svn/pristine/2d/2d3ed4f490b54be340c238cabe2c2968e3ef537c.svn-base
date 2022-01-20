#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "core.h"

#include <uv.h>

#define HTTP_HEADER_DEFAULT_COUNT   64
#define HTTP_HEADER_REHASH_THRESHOLD   0.05

#define HTTP_HEADER_MAX_KEY_LENGTH     256
#define HTTP_HEADER_MAX_VALUE_LENGTH   2048

#define HTTP_BODY_MAX_LENGTH    2048
// 1M is big enough in our approach
#define HTTP_MESSAGE_MAX_LENGTH    1024 * 1024

#define HTTP_MIN_STATUS_CODE    100
#define HTTP_MAX_STATUS_CODE    599

#define BYPASS_PROXY_HEADER_LEN 5
extern  const char* BYPASS_PROXY_HEADER[];

static const char HTTP_DEFAULT_VERSION[] = "HTTP/1.1";
static const char HTTP_DEFAULT_AUTH[] = "Basic";
static const char HTTP_DEFAULT_REALM[] = "rps";
static const char HTTP_DEFAULT_PROXY_AGENT[] = "RPS/1.0";
static const char HTTP_DEFAULT_PROXY_CONNECTION[] = "Keep-Alive";
static const char HTTP_DEFAULT_CONNECTION[] = "close";


#define HTTP_RESP_MAP(V)                                                \
    V(0,   http_undefine, "Undefine")                                   \
    V(200, http_ok, "OK")                                               \
    V(301, http_moved_permanently, "Moved Permanently")                 \
    V(302, http_found, "Moved Temporarily")                             \
    V(304, http_not_modified, "Not Modified")                           \
    V(400, http_bad_request, "Bad Request")                             \
    V(403, http_forbidden, "Forbidden")                                 \
    V(404, http_not_found, "Not Found")                                 \
    V(405, http_method_not_allowed, "Method Not Allowed")               \
    V(407, http_proxy_auth_required, "Proxy Authentication Required")   \
    V(408, http_request_timeout, "Request Timeout")                     \
    V(500, http_server_error, "Internal Server Error")                  \
    V(502, http_bad_gateway, "Bad Gateway")                             \
    V(503, http_proxy_unavailable, "Proxy Unavailable")                 \

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

#define HTTP_REPLY_CODE_MAP(V)                                      \
    V(http_ok,                  rps_rep_ok)                         \
    V(http_moved_permanently,   rps_rep_moved_permanent)            \
    V(http_found,               rps_rep_moved_temporary)            \
    V(http_not_modified,        rps_rep_not_modified)               \
    V(http_forbidden,           rps_rep_forbidden)                  \
    V(http_not_found,           rps_rep_not_found)                  \
    V(http_proxy_auth_required, rps_rep_auth_require)               \
    V(http_request_timeout,     rps_rep_timeout)                    \
    V(http_server_error,        rps_rep_server_error)               \
    V(http_bad_gateway,         rps_rep_unreachable)                \
    V(http_proxy_unavailable,   rps_rep_proxy_unavailable)          \


static inline int
http_reply_code_lookup(int code) {
    #define HTTP_REPLY_CODE_GEN(c1, c2) case c1: return c2;
    switch(code) {
        HTTP_REPLY_CODE_MAP(HTTP_REPLY_CODE_GEN)
        default: ;
    }
    #undef HTTP_REPLY_CODE_GEN
    return rps_rep_undefined;
}

static inline int
http_reply_code_reverse(int code) {
    #define HTTP_REPLY_CODE_GEN(c1, c2) case c2: return c1;
    switch(code) {
        HTTP_REPLY_CODE_MAP(HTTP_REPLY_CODE_GEN)
        default: ;
    }
    #undef HTTP_REPLY_CODE_GEN
    return http_undefine;
}


#define HTTP_METHOD_MAP(V)                  \
    V(0, http_emethod, "EMETHOD")           \
    V(1, http_get, "GET")                   \
    V(2, http_post, "POST")                 \
    V(3, http_put, "PUT")                   \
    V(4, http_head, "HEAD")                 \
    V(5, http_connect, "CONNECT")           \

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

static inline int
http_valid_code(uint16_t code) {
    return (code > HTTP_MIN_STATUS_CODE) && (code < HTTP_MAX_STATUS_CODE);
}

enum http_request_verify_result {
    http_verify_error = -1,
    http_verify_fail = 0,
    http_verify_success = 1,
};


enum http_auth_schema {
    http_auth_unknown = 0,
    http_auth_basic = 1,
    http_auth_digest,
};

enum http_recv_send {
    http_recv,
    http_send,
};

struct http_request_auth {
    uint8_t             schema;
    rps_str_t           param;
};

struct http_request {
    uint8_t             method;
    rps_str_t           full_uri;
    rps_str_t           schema;
    rps_str_t           host;
    int                 port;
    rps_str_t           path;
    rps_str_t           params;
    rps_str_t           version;
    rps_str_t           body;
    rps_hashmap_t       headers;        
    
};

struct http_response {
    uint16_t            code;
    rps_str_t           status;
    rps_str_t           version;
    rps_str_t           body;
    rps_hashmap_t       headers;        
};


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
rps_status_t http_response_parse(struct http_response *resp, uint8_t *data, size_t size);

int http_basic_auth(struct context *ctx, rps_str_t *param);
int http_basic_auth_gen(const char *uname, const char *passwd, char *output);

#ifdef RPS_DEBUG_OPEN
void http_request_dump(struct http_request *req, uint8_t rs);
void http_response_dump(struct http_response *resp, uint8_t rs);
#endif

int http_request_message(char *message, struct http_request *req);
int http_response_message(char *message, struct http_response *resp);

int http_request_verify(struct context *ctx);
int http_response_verify(struct context *ctx);
rps_status_t http_send_response(struct context *ctx, uint16_t code);
rps_status_t http_send_request(struct context *ctx);

#endif
