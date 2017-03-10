#include "s5.h"
#include "core.h"

#include <stdio.h>




static void
s5_do_handshake(struct context *ctx) {
    rps_status_t status;
    struct session  *sess;
    struct s5_method_request req;

    sess = ctx->sess;

    req.ver = SOCKS5_VERSION;
    if (string_empty(&sess->upstream.uname)) {
        req.nmethods = 1;
        req.methods[0] = 0x00;
        status = server_write(ctx, &req, 3);
    } else {
        req.nmethods = 2;
        req.methods[0] = 0x00;
        req.methods[1] = 0x02;
        status = server_write(ctx, &req, 4);
    }

    if (status != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }
    
    ctx->state = c_handshake_reply;
    return;
}

static void
s5_do_handshake_reply(struct context *ctx) {
    uint8_t    *data;
    size_t     size;
    ctx_state_t new_state;
    struct s5_method_response *resp;
    
    data = (uint8_t *)ctx->buf;
    size = (size_t)ctx->nread;

    resp = (struct s5_method_response *)data;
    if (resp->ver != SOCKS5_VERSION) {
        log_error("s5 handshake error: bad protocol version.");
        goto kill;
    }

    if (size != 2) {
        log_error("junk in handshake");
        goto kill;
    }

    switch (resp->method) {
        case s5_auth_none:
            new_state = c_requests;
            break;
        case s5_auth_passwd:
            new_state = c_auth;
            break;
        case s5_auth_gssapi:
        case s5_auth_unacceptable:
        default:
            new_state = c_kill;
            log_error("s5 handshake error: unacceptable authentication.");
            break;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("s5 client handshake finish.");
#endif

    ctx->state = new_state;
    server_do_next(ctx);
    return;

kill:
    ctx->state = c_kill;
    server_do_next(ctx);
}

static void
s5_do_auth(struct context *ctx) {
    printf("begin do auth\n");
    
}


static void
s5_do_request(struct context *ctx) {
    printf("begin do request\n");
}


void 
s5_client_do_next(struct context *ctx) {

    switch(ctx->state) {
        case c_handshake:
            s5_do_handshake(ctx);
            break;
        case c_handshake_reply:
            s5_do_handshake_reply(ctx);
            break;
        case c_auth:
            s5_do_auth(ctx);
            break;
        case c_requests:
            s5_do_request(ctx);
            break;
        default:
            NOT_REACHED();
    }
    
}
