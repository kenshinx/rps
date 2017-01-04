#include "private.h"
#include "server.h"

#include <uv.h>

void 
private_do_parse(struct context *ctx, const char *data, ssize_t nread) {
    printf("<<nread:%zd>>, %s\n", nread, data);
}
