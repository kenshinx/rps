#include "http.h"
#include "core.h"

#include <uv.h>


ctx_state_t 
http_server_do_next(struct context *ctx) {
    char    *data;
    ssize_t size;

    data = ctx->buf;
    size = ctx->nread;

    printf("http read %zd bytes: %s\n", size, data);
}

ctx_state_t 
http_client_do_next(struct context *ctx) {
}

ctx_state_t
http_server_handshake(http_handle_t *handle) {
    
}

ctx_state_t
http_server_auth(http_handle_t *handle) {

}

ctx_state_t
http_client_handshake(http_handle_t *handle) {
    
}

ctx_state_t
http_client_auth(http_handle_t *handle) {

}

