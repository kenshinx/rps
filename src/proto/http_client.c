
#include "http.h"
#include "core.h"

#include <uv.h>

static void
http_do_handshake(struct context *ctx) {
    struct http_request req;

    http_request_init(&req);
    
    req.method = http_get;
    

}


void
http_client_do_next(struct context *ctx) {
    
    switch (ctx->state) {
        case c_handshake_req:
            http_do_handshake(ctx);
            break;

        default:
            NOT_REACHED();
    }
}
