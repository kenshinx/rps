#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "core.h"

#include <uv.h>

/*
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 */


void http_server_do_next(struct context *ctx);
void http_client_do_next(struct context *ctx);

#endif
