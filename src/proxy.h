#ifndef _PROXY_H
#define _PROXY_H

#include "core.h"
#include "util.h"
#include "array.h"
#include "_string.h"

struct proxy  {
    rps_addr_t  server;
    rps_str_t   uname;
    rps_str_t   passwd;
    rps_proto_t proto;
};

struct proxy_pool {
    rps_array_t pool;
    uint32_t    index;
};

void proxy_init(struct proxy *p);

#endif
