#include "http.h"
#include "http_tunnel.h"

static void
http_tunnel_do_handshake(struct context *ctx) {
    if (http_send_request(ctx) != RPS_OK) {
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
