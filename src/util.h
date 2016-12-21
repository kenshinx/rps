#ifndef _RPS_UTIL_H
#define _RPS_UTIL_H

#include "log.h"

#include "uv.h"

#include <stdlib.h>

#define CRLF    "\x0d\x0a"

#define MAX(_a, _b)     ((_a) > (_b) ? (_a):(_b))

#define rps_alloc(_s)                                               \
    _rps_alloc((size_t)(_s), __FILE__, __LINE__)                    \

#define rps_zalloc(_s)                                              \
    _rps_zalloc((size_t)(_s), __FILE__, __LINE__)                   \

#define rps_calloc(_n, _s)                                          \
    _rps_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)     \

#define rps_realloc(_p, _s)                                         \
    _rps_realloc(_p, (size_t)(_s), __FILE__, __LINE__)              \

#define rps_free(_p)                                                \
    _rps_free(_p, __FILE__, __LINE__)                               \


void  *_rps_alloc(size_t size, const char *name, int line);
void * _rps_zalloc(size_t size, const char *name, int line);
void *_rps_calloc(size_t nmemb, size_t size, const char *name, int line);
void *_rps_realloc(void *ptr, size_t size, const char *name, int line);
void _rps_free(void *ptr, const char *name, int line);


#define UV_SHOW_ERROR(err, why) do {                                \
    log_error("libuv %s:%s", why, uv_strerror(err));                 \
} while(0)


#define NOT_REACHED() rps_assert("not reached", __FILE__, __LINE__);                                      
#define ASSERT(_x) do {                                             \
    if (!(_x)) {                                                    \
        rps_assert(#_x, __FILE__, __LINE__);                        \
    }                                                               \
} while(0)

void rps_assert(const char *cond, const char *file, int line);

#endif

