#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include "server.h"

#include <uv.h>

void 
http_do_parse(struct context *ctx, const char *data, ssize_t nread);

#endif
