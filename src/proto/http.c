#include "http.h"
#include "core.h"

#include <uv.h>


void 
http_server_do_next(struct context *ctx) {
    char    *data;
    ssize_t size;
    http_handle_t   *handle;

    handle = &ctx->proxy_handle.http;

    data = ctx->buf;
    size = ctx->nread;

    printf("http read %zd bytes: %s\n", size, data);
}

void 
http_client_do_next(struct context *ctx) {
}

uint16_t
http_server_handshake(http_handle_t *handle) {
    
}

uint16_t
http_server_auth(http_handle_t *handle) {

}

uint16_t
http_client_handshake(http_handle_t *handle) {
    
}

uint16_t
http_client_auth(http_handle_t *handle) {

}

