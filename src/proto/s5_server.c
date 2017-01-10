
#include "s5.h"
#include "server.h"

#include <stdio.h>
#include <uv.h>



void 
s5_do_next(struct context *ctx, const char *data, ssize_t nread) {
    printf("<<nread:%zd>>, %s\n", nread, data);
}

static void 
s5_parse(char **data, ssize_t *size) {

}

uint16_t
s5_server_handshake(struct context *ctx) {
    char    *data;
    ssize_t size;

    data = ctx->buf;
    size = ctx->nread;

    s5_parse(&data, &size);
}

uint16_t
s5_server_auth(struct context *ctx) {

}

static s5_state_t 
s5_request() {

}

static s5_state_t
s5_reply() {

}

