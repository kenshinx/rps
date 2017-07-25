#include "http.h"
#include "http_tunnel.h"

static int
http_tunnel_send_proxy(struct context *ctx) {
    struct http_request req;
    struct upstream *u;
    rps_addr_t  *remote;
    char remote_addr[MAX_INET_ADDRSTRLEN];
    char message[HTTP_MESSAGE_MAX_LENGTH];
    int len;

    remote = &ctx->sess->remote;

    if (rps_unresolve_addr(remote, remote_addr) != RPS_OK) {
        return RPS_ERROR;
    }


    http_request_init(&req);
    
    req.method = http_connect;
    req.port = (int)rps_unresolve_port(remote);
    string_duplicate(&req.host, remote_addr, strlen(remote_addr));
    string_duplicate(&req.version, HTTP_DEFAULT_VERSION, strlen(HTTP_DEFAULT_VERSION));

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
        const char key3[] = "Proxy-Authorization";
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

    const char key5[] = "Connection";
    const char val5[] = "close";
    hashmap_set(&req.headers, (void *)key5, strlen(key5), 
            (void *)val5, strlen(val5));

    len = http_request_message(message, &req);

    ASSERT(len > 0);

    http_request_deinit(&req);

    return server_write(ctx, message, len);
}

static void
http_tunnel_do_handshake(struct context *ctx) {
    if (http_tunnel_send_proxy(ctx) != RPS_OK) {
        ctx->state = c_retry;
        server_do_next(ctx);
    } else {
        ctx->state = c_handshake_resp;
    }
}

static void
http_tunnel_do_handshake_resp(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_response_verify(ctx);

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
http_tunnel_do_auth(struct context *ctx) {
    struct upstream *u;

    u = &ctx->sess->upstream;

    if (string_empty(&u->uname)) {
        goto retry;
    }

    if (http_tunnel_send_proxy(ctx) != RPS_OK) {
        goto retry;
    }

    ctx->state = c_auth_resp;
    return;

retry:
    ctx->state = c_retry;
    server_do_next(ctx);
}

static void
http_tunnel_do_auth_resp(struct context *ctx) {
    int http_verify_result;
    
    http_verify_result = http_response_verify(ctx);

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
http_tunnel_client_do_next(struct context *ctx) {
    
    switch (ctx->state) {
    case c_handshake_req:
        http_tunnel_do_handshake(ctx);
        break;
    case c_handshake_resp:
        http_tunnel_do_handshake_resp(ctx);
        break;
    case c_auth_req:
        http_tunnel_do_auth(ctx);
        break;
    case c_auth_resp:
        http_tunnel_do_auth_resp(ctx);
        break;
    case c_closing:
        break;
    default:
        NOT_REACHED();
    }
}
