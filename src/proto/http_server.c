#include "http.h"
#include "core.h"

#include <uv.h>



static void
http_do_handshake(struct context *ctx) {
    
}

static void
http_do_auth(struct context *ctx) {

}


void
http_server_do_next(struct context *ctx) {
    char    *data;
    ssize_t size;

    data = ctx->rbuf;
    size = ctx->nread;

    printf("http read %zd bytes: %s\n", size, data);
}

