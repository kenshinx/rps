#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "server.h"

#include <uv.h>

/*
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 */

void 
http_do_next(struct context *ctx, const char *data, ssize_t nread);

#endif
