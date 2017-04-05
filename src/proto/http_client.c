
#include "http.h"
#include "core.h"

#include <uv.h>

static int
http_send_request(struct context *ctx) {
    struct http_request req;
    rps_addr_t  *remote;
    struct upstream *u;
    char message[HTTP_MESSAGE_MAX_LENGTH];
    int len;

    remote = &ctx->sess->remote;
    ASSERT(remote->family == AF_DOMAIN);

    http_request_init(&req);
    
    req.method = http_connect;
    req.port = (int)remote->addr.name.port;
    string_duplicate(&req.host, remote->addr.name.host, strlen(remote->addr.name.host));
    string_duplicate(&req.protocol, HTTP_DEFAULT_PROTOCOL, strlen(HTTP_DEFAULT_PROTOCOL));

    const char key1[] = "Host";
    char val1[HTTP_HEADER_MAX_VALUE_LENGTH];
    int v1len;
    
    v1len = snprintf(val1, HTTP_HEADER_MAX_VALUE_LENGTH, "%s:%d", req.host.data, req.port);
    hashmap_set(&req.headers, (void *)key1, strlen(key1), (void *)val1, v1len);

#ifdef HTTP_PROXY_AGENT
    const char key2[] = "Proxy-Agent";
    hashmap_set(&req.headers, (void *)key2, strlen(key2), 
            (void *)HTTP_DEFAULT_PROXY_AGENT, strlen(HTTP_DEFAULT_PROXY_AGENT));
#endif

    u = &ctx->sess->upstream;
    
    if (!string_empty(&u->uname)) {
        /* autentication required */
        const char key3[] = "Proxy-Authenticate";
        char val3[HTTP_HEADER_MAX_VALUE_LENGTH];   
        int v3len;
        
        v3len = http_basic_auth_gen((const char *)u->uname.data, 
                (const char *)u->passwd.data, val3);
        hashmap_set(&req.headers, (void *)key3, strlen(key3), (void *)val3, v3len);
    }

    len = http_request_message(message, &req);

    ASSERT(len > 0);

    http_request_deinit(&req);

    return server_write(ctx, message, len);
}

static void
http_do_handshake(struct context *ctx) {
    if (http_send_request(ctx) != RPS_OK) {
        ctx->state = c_retry;
        server_do_next(ctx);
    } else {
        ctx->state = c_handshake_resp;
    }
}

static void
http_do_handshake_resp(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    rps_status_t status;
    struct http_response resp;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    http_response_init(&resp);

    status = http_response_parse(&resp, data, size);
    if (status != RPS_OK) {
        ctx->state = c_retry;
        server_do_next(ctx);
        return;
    }

}


void
http_client_do_next(struct context *ctx) {
    
    switch (ctx->state) {
        case c_handshake_req:
            http_do_handshake(ctx);
            break;
        case c_handshake_resp:
            http_do_handshake_resp(ctx);
            break;


        default:
            NOT_REACHED();
    }
}
