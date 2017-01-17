#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include <uv.h>

/*
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 */


typedef struct {
    void    *data;
    uint8_t t;
    
} http_handle_t;

#include "core.h"

static inline void
http_handle_init(http_handle_t *handle) {
    memset(handle, 0, sizeof(*handle));
}

void http_server_do_next(struct context *ctx);
void http_client_do_next(struct context *ctx);

#endif
