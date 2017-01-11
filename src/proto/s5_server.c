
#include "s5.h"
#include "core.h"

#include <stdio.h>
#include <uv.h>



void 
s5_server_do_next(s5_handle_t *handle) {
}

static void 
s5_parse(char **data, ssize_t *size) {

}

uint16_t
s5_server_handshake(s5_handle_t *handle) {
    struct context *ctx;
    char    *data;
    ssize_t size;

    ctx = (struct context *)handle->data;

    data = ctx->buf;
    size = ctx->nread;

    s5_parse(&data, &size);
}

uint16_t
s5_server_auth(s5_handle_t *handle) {

}

static s5_state_t 
s5_request() {

}

static s5_state_t
s5_reply() {

}

