#include "s5.h"
#include "core.h"

#include <stdio.h>




static ctx_state_t
s5_do_handshake(struct context *ctx) {
    rps_status_t status;
    ctx_state_t new_state;
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
        return c_kill;
    }
    
    return c_handshake_reply;
}

static ctx_state_t
s5_do_handshake_reply(struct context *ctx) {
    printf("cliet into do handshake reply phase\n");
    return c_requests;
}



static ctx_state_t
s5_do_auth() {

}


ctx_state_t 
s5_client_do_next(struct context *ctx) {
    ctx_state_t new_state;

    switch(ctx->state) {
        case c_handshake:
            new_state = s5_do_handshake(ctx);
            break;
        case c_handshake_reply:
            new_state = s5_do_handshake_reply(ctx);
            break;
        default:
            NOT_REACHED();
            new_state = c_kill;
    }

    return new_state;
    
}
