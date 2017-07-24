#include "http.h"
#include "http_tunnel.h"


static void
http_do_handshake(struct context *ctx) {
    int http_verify_result;
    struct http_request *req;

    http_verify_result = http_request_verify(ctx);

    req = (struct http_request *)ctx->req;
    if (req == NULL) {
        log_verb("http tunnel client handshake error");
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

    switch (http_verify_result) {
    case http_verify_error:
        log_verb("http tunnel client handshake '%s %s' error", 
                http_method_str(req->method), req->full_uri.data);
        ctx->state = c_kill;
        break;
    case http_verify_success:
        ctx->state = c_exchange;
        log_verb("http tunnel client handshake '%s %s' success", 
                http_method_str(req->method), req->full_uri.data);
        break;
    case http_verify_fail:
        ctx->state = c_handshake_resp;
        log_verb("http tunnel client handshake '%s %s' need authentication", 
                http_method_str(req->method), req->full_uri.data);
        break;
    }


    http_request_deinit(ctx->req);
    rps_free(ctx->req);
    ctx->req = NULL;
    server_do_next(ctx);
}

static void
http_do_handshake_resp(struct context *ctx) {
    if (http_send_response(ctx, http_proxy_auth_required) != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
    } else {
        ctx->state = c_auth_req;
    }
}

static void
http_do_auth(struct context *ctx) {
    int http_verify_result;
    struct http_request *req;

    http_verify_result = http_request_verify(ctx);

    req = (struct http_request *)ctx->req;
    if (req == NULL) {
        log_verb("http  tunnel client authentication error");
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

    switch (http_verify_result) {
    case http_verify_success:
        log_debug("http tunnel client '%s %s' authenticate success",
                    http_method_str(req->method), req->full_uri.data);
        ctx->state = c_exchange;
        break;
    case http_verify_fail:
        ctx->state = c_auth_resp;
        log_debug("http tunnel client '%s %s' authenticate fail",
                    http_method_str(req->method), req->full_uri.data);
        break;
    case http_verify_error:
        ctx->state = c_kill;
        log_debug("http tunnel client '%s %s' authenticate error", 
                    http_method_str(req->method), req->full_uri.data);
        break;
    }

    http_request_deinit(ctx->req);
    rps_free(ctx->req);
    ctx->req = NULL;

    server_do_next(ctx);
}

static void
http_do_auth_resp(struct context *ctx) {
    http_send_response(ctx, http_proxy_auth_required);
    ctx->state = c_kill;
    server_do_next(ctx);
}

static void
http_do_reply(struct context *ctx) {
    int code;

    /* traslate rps unified reply code to http response code */
    code = http_reply_code_reverse(ctx->reply_code);

    if (code == http_undefine) {
        code = http_server_error;
    }

    if (http_send_response(ctx, code) != RPS_OK) {
        ctx->established = 0;
        ctx->state = c_kill;
        return;
    }

    if (code != http_ok) {
        ctx->established = 0;
        ctx->state = c_kill;
    } else {
        ctx->established = 1;
        ctx->state = c_established;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("http client reply, connect remote %s", http_resp_code_str(code));
#endif

}


void
http_tunnel_server_do_next(struct context *ctx) {

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
    case c_reply:
        http_do_reply(ctx);
        break;
    default:
        NOT_REACHED();
    }
}

