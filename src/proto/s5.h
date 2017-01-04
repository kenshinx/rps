#ifndef _RPS_S5_H
#define _RPS_S5_H

#include "server.h"

#include <uv.h>


void s5_do_parse(struct context *ctx, const char *data, ssize_t nread);

#endif
