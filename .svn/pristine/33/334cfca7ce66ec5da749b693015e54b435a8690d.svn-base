#include "http.h"
#include "http_tunnel.h"


static rps_status_t
http_tunnel_send_request(struct context *ctx) {
    struct http_request *req, nreq;
    struct upstream *u;
    size_t i;
    char message[HTTP_MESSAGE_MAX_LENGTH];
    int len;

    req = ctx->sess->request->req;

    ASSERT(req != NULL);

    http_request_init(&nreq);
    nreq.method = http_connect;
    nreq.port = req->port;
    string_copy(&nreq.host, &req->host);
    string_copy(&nreq.version, &req->version);
    string_copy(&nreq.full_uri, &req->full_uri);
    hashmap_deepcopy(&nreq.headers, &req->headers);
    

    for (i = 0; i < BYPASS_PROXY_HEADER_LEN; i++) {
        hashmap_remove(&nreq.headers, (void *)BYPASS_PROXY_HEADER[i], 
                strlen(BYPASS_PROXY_HEADER[i]));
    } 

#ifdef HTTP_PROXY_REDEFINE_HOST_HEADER
    const char key1[] = "host";
    char val1[HTTP_HEADER_MAX_VALUE_LENGTH];
    int v1len;
    v1len = snprintf(val1, HTTP_HEADER_MAX_VALUE_LENGTH, "%s:%d", nreq.host.data, nreq.port);
    hashmap_set(&nreq.headers, (void *)key1, strlen(key1), (void *)val1, v1len);
#endif

    u = ctx->sess->upstream;
    
    if (!string_empty(&u->uname)) {
        /* autentication required */
        const char key2[] = "Proxy-Authorization";
        char val2[HTTP_HEADER_MAX_VALUE_LENGTH];   
        int vlen2;
        
        vlen2 = http_basic_auth_gen((const char *)u->uname.data, 
                (const char *)u->passwd.data, val2);
        hashmap_set(&nreq.headers, (void *)key2, strlen(key2), (void *)val2, vlen2);
    }
        
#ifdef HTTP_PROXY_CONNECTION
    /* set proxy-connection header*/
    const char key3[] = "Porxy-Connection";
    hashmap_set(&nreq.headers, (void *)key3, strlen(key3), 
            (void *)HTTP_DEFAULT_PROXY_CONNECTION, strlen(HTTP_DEFAULT_PROXY_CONNECTION));
#endif

#ifdef HTTP_PROXY_AGENT
    const char key4[] = "Proxy-Agent";
    hashmap_set(&nreq.headers, (void *)key4, strlen(key4), 
            (void *)HTTP_DEFAULT_PROXY_AGENT, strlen(HTTP_DEFAULT_PROXY_AGENT));
#endif
    
    
    len = http_request_message(message, &nreq);

    ASSERT(len > 0);

    http_request_deinit(&nreq);

    return server_write(ctx, message, len);
}

static void
http_tunnel_do_handshake(struct context *ctx) {
    if (http_tunnel_send_request(ctx) != RPS_OK) {
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
        ctx->state = c_establish;
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

    u = ctx->sess->upstream;

    if (string_empty(&u->uname)) {
        goto retry;
    }

    if (http_tunnel_send_request(ctx) != RPS_OK) {
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
        ctx->state = c_establish;
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
