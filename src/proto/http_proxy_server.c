#include "http.h"
#include "http_proxy.h"

static void
http_proxy_parse_request(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_request_verify(ctx);

    switch (http_verify_result) {
    case http_verify_error:
        log_verb("http client request error");
        ctx->state = c_kill;
        break;
    case http_verify_success:
        ctx->state = c_exchange;
        log_verb("http client request success");
        break;
    case http_verify_fail:
        ctx->state = c_auth_resp;
        log_verb("http client request authentication required");
        break;
    }

    server_do_next(ctx);
}

static void
http_proxy_send_auth(struct context *ctx) {
    if (http_send_response(ctx, http_proxy_auth_required) != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
    } else {
        ctx->state = c_requests;
    }
}


void
http_proxy_server_do_next(struct context *ctx) {
    switch (ctx->state) {
    case c_handshake_req:
    case c_requests:
        http_proxy_parse_request(ctx);
        break;
    case c_auth_resp:
        http_proxy_send_auth(ctx);
        break;
    default:
        NOT_REACHED();
    }

}

