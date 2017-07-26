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
    struct context *request, *forward;
    struct session *sess;
    char remoteip[MAX_INET_ADDRSTRLEN];

    sess = ctx->sess;
    request = sess->request;
    forward = sess->forward;

    rps_unresolve_addr(&sess->remote, remoteip);

    http_verify_result = http_response_verify(ctx);
    switch (http_verify_result) {
    case http_verify_success:
        ctx->state = c_pipelined;
        log_info("Establish pipeline %s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d.",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote));
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

