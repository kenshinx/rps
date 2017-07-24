#include "http.h"
#include "http_proxy.h"

static void
http_proxy_parse_request(struct context *ctx) {
    int http_verify_result;
    struct http_request *req;

    http_verify_result = http_request_verify(ctx);

    if (ctx->req == NULL) {
        log_verb("http proxy client request error");
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

    req = (struct http_request *)ctx->req;

    switch (http_verify_result) {
    case http_verify_error:
        ctx->state = c_kill;
        log_verb("http proxy client %s %s error", 
                http_method_str(req->method), req->full_uri.data);
        break;
    case http_verify_fail:
        ctx->state = c_auth_resp;
        log_verb("http proxy client %s %s need authentication", 
                http_method_str(req->method), req->full_uri.data);
        break;
    case http_verify_success:
        ctx->state = c_exchange;
        log_verb("http proxy client %s %s success", 
                http_method_str(req->method), req->full_uri.data);
        server_do_next(ctx);
        return;
    }

    http_request_deinit(ctx->req);
    rps_free(ctx->req);
    ctx->req = NULL;
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

