#include "http.h"
#include "http_proxy.h"


const char* BYPASS_PROXY_HEADER[] = {
    "proxy-authorization",
    "proxy-connection",
    "transfer-encoding",
    "connection",
    "upgrade"
};

#define BYPASS_PROXY_HEADER_LEN  (sizeof(BYPASS_PROXY_HEADER)/sizeof(BYPASS_PROXY_HEADER[0]))


static void
http_proxy_send_request(struct context *ctx) {
    struct http_request *req;
    struct upstream *u;
    size_t i;
    char message[HTTP_MESSAGE_MAX_LENGTH];
    int len;

    req = ctx->sess->request->req;

    ASSERT(req != NULL);
    
    for (i = 0; i < BYPASS_PROXY_HEADER_LEN; i++) {
        hashmap_remove(&req->headers, (void *)BYPASS_PROXY_HEADER[i], 
                strlen(BYPASS_PROXY_HEADER[i]));
    } 

    u = &ctx->sess->upstream;
    
    if (!string_empty(&u->uname)) {
        /* autentication required */
        const char key[] = "Proxy-Authorization";
        char val[HTTP_HEADER_MAX_VALUE_LENGTH];   
        int vlen;
        
        vlen = http_basic_auth_gen((const char *)u->uname.data, 
                (const char *)u->passwd.data, val);
        hashmap_set(&req->headers, (void *)key, strlen(key), (void *)val, vlen);
    }
        
#ifdef HTTP_PROXY_CONNECTION
    /* set proxy-connection header*/
    const char key2[] = "Porxy-Connection";
    hashmap_set(&req->headers, (void *)key2, strlen(key2), 
            (void *)HTTP_DEFAULT_PROXY_CONNECTION, strlen(HTTP_DEFAULT_PROXY_CONNECTION));
#endif

    const char key3[] = "Connection";
    const char val3[] = "close";
    hashmap_set(&req->headers, (void *)key3, strlen(key3), 
            (void *)val3, strlen(val3));

    len = http_request_message(message, req);

    ASSERT(len > 0);

    if (server_write(ctx, message, len) != RPS_OK) {
        ctx->state = c_retry;
        server_do_next(ctx);
        return;
    } 
    
    ctx->state = c_reply;

    /*
    http_request_deinit(req);
    rps_free(ctx->sess->request->req);
    ctx->sess->request->req = NULL;
    */
}

static void
http_proxy_parse_response(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_response_verify(ctx);
    switch (http_verify_result) {
    case http_verify_success:
        ctx->state = c_pipelined;
        break;
    case http_verify_fail:
    case http_verify_error:
        ctx->state = c_retry;
        break;
    }

    server_do_next(ctx);
    return;
}

void
http_proxy_client_do_next(struct context *ctx) {
    switch (ctx->state) {
    case c_handshake_req:
        http_proxy_send_request(ctx);
        break;
    case c_reply:
        http_proxy_parse_response(ctx);
        break;
    default:
        NOT_REACHED();
    }
}

