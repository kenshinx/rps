
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

#ifdef HTTP_PROXY_CONNECTION
    /* set proxy-connection header*/
    const char key4[] = "Porxy-Connection";
    hashmap_set(&req.headers, (void *)key4, strlen(key4), 
            (void *)HTTP_DEFAULT_PROXY_CONNECTION, strlen(HTTP_DEFAULT_PROXY_CONNECTION));
    
#endif


    len = http_request_message(message, &req);

    ASSERT(len > 0);

    http_request_deinit(&req);

    return server_write(ctx, message, len);
}

static int
http_verify_response(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    rps_status_t status;
    struct http_response resp;
    int result;
    char remoteip[MAX_INET_ADDRSTRLEN];

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    http_response_init(&resp);

    status = http_response_parse(&resp, data, size);
    if (status != RPS_OK) {
        log_debug("http upstream %s return invalid response", ctx->peername);
        return http_verify_error;
    }

    rps_unresolve_addr(&ctx->sess->remote, remoteip);

    ctx->reply_code = resp.code;

    switch (resp.code) {
        case http_ok:
            result = http_verify_success;
#ifdef RPS_DEBUG_OPEN
            log_verb("http upstream %s connect remote %s success", 
                    ctx->peername, remoteip);
#endif
            break;

        case http_proxy_auth_required:
            log_debug("http upstream %s 407 authentication failed", 
                    ctx->peername);
            result = http_verify_fail;
            break;

        case http_forbidden:
        case http_not_found:
        case http_server_error:
        case http_bad_gateway:
            log_debug("http upstream %s error, %d %s", ctx->peername, 
                    resp.code, resp.status.data);
            result = http_verify_error;
            break;

        default:
            log_debug("http upstream %s return undefined status code, %s", 
                    ctx->peername, resp.status.data);
            result = http_verify_error;
    }

    return result;


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
    int http_verify_result;

    http_verify_result = http_verify_response(ctx);

    switch (http_verify_result) {
        case http_verify_success:
            ctx->established = 1;
            ctx->state = c_exchange;
            break;
        case http_verify_fail:
#ifdef RPS_HTTP_CLIENT_REAUTH
            ctx->state = c_auth_req;
            break;
#endif
        case http_verify_error:
            ctx->state = c_retry;
            break;

        default:
            NOT_REACHED();
             
    }

    server_do_next(ctx);
}

static void
http_do_auth(struct context *ctx) {
    struct upstream *u;

    u = &ctx->sess->upstream;

    if (string_empty(&u->uname)) {
        goto retry;
    }

    if (http_send_request(ctx) != RPS_OK) {
        goto retry;
    }

    ctx->state = c_auth_resp;
    return;

retry:
    ctx->state = c_retry;
    server_do_next(ctx);
}

static void
http_do_auth_resp(struct context *ctx) {
    int http_verify_result;
    
    http_verify_result = http_verify_response(ctx);

    switch (http_verify_result) {
        case http_verify_success:
            ctx->established = 1;
            ctx->state = c_exchange;
            break;
        case http_verify_fail:
        case http_verify_error:
            ctx->state = c_retry;
            break;
        default:
            NOT_REACHED();
    }

    server_do_next(ctx);
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
        case c_auth_req:
            http_do_auth(ctx);
            break;
        case c_auth_resp:
            http_do_auth_resp(ctx);
            break;
        default:
            NOT_REACHED();
    }
}
