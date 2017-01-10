#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "core.h"

#include <uv.h>

/*
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 */

struct http_handle_s {
    uint8_t t;
    
};

typedef struct http_handle_s http_handle_t;

void 
http_do_next(struct context *ctx, const char *data, ssize_t nread);

#endif
