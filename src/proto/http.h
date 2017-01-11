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

void http_server_do_next(struct context *ctx);
void http_client_do_next(struct context *ctx);

#endif
