#include "http.h"
#include "http_proxy.h"


static void
http_proxy_do_request(struct context *ctx) {

    if (http_send_request(ctx) != RPS_OK) {
        ctx->state = c_retry;
        server_do_next(ctx);
        return;
    } 
    
    ctx->state = c_reply;
}

static void
http_proxy_do_response(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_response_verify(ctx);
    switch (http_verify_result) {
    case http_verify_success:
        ctx->state = c_establish;
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
        http_proxy_do_request(ctx);
        break;
    case c_reply:
        http_proxy_do_response(ctx);
        break;
    case c_closing:
        break;
    default:
        NOT_REACHED();
    }
}

