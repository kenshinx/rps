
#include "s5.h"
#include "core.h"
#include "util.h"

#include <stdio.h>
#include <uv.h>




static void 
s5_parse(char **data, ssize_t *size) {

}

static uint16_t
s5_do_handshake(s5_handle_t *handle, const char *data, ssize_t size) {
    printf("s5 read %d bytes: %s\n", size, data);

}

static uint16_t
s5_do_auth(s5_handle_t *handle, const char *data, ssize_t size) {

}

static s5_state_t 
s5_request() {

}

static s5_state_t
s5_reply() {

}

void 
s5_server_do_next(struct context *ctx) {
    char    *data;
    ssize_t size;
    uint16_t new_state; 
    s5_handle_t *handle;

    handle = &ctx->proxy_handle.s5;

    data = ctx->buf;
    size = ctx->nread;

    switch (ctx->state) {
        case c_handshake:
            new_state = s5_do_handshake(handle, data, size);
            break;
        case c_auth:
            new_state = s5_do_auth(handle, data, size);
            break;
        default:
            NOT_REACHED();
    }
    
    ctx->state = new_state;
}
