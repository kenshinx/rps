#include "http.h"
#include "core.h"

#include <uv.h>



static void
http_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    
    printf("begin do http handshake, read %zd bytes: %s\n", size, data);
}

static void
http_do_auth(struct context *ctx, uint8_t *data, size_t size) {
    printf("begin do http auth, read %zd bytes: %s\n", size, data);

}


void
http_server_do_next(struct context *ctx) {
    uint8_t *data;
    ssize_t size;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;


    switch (ctx->state) {
        case c_handshake:
            http_do_handshake(ctx, data, size);
            break;
        case c_auth:
            http_do_auth(ctx, data, size);
            break;
        default:
            NOT_REACHED();
    }
}

