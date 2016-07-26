#ifndef _RPG_UTIL_H
#define _RPG_UTIL_H

#include <stdlib.h>

#define CRLF    "\x0d\x0a"

#define MAX(_a, _b)     ((_a) > (_b) ? (_a):(_b))

#define rpg_alloc(_s)                                               \
    _rpg_alloc((size_t)(_s), __FILE__, __LINE__)                    \

#define rpg_zalloc(_s)                                              \
    _rpg_zalloc((size_t)(_s), __FILE__, __LINE__)                   \

#define rpg_calloc(_n, _s)                                          \
    _rpg_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)     \

#define rpg_realloc(_p, _s)                                         \
    _rpg_realloc(_p, (size_t)(_s), __FILE__, __LINE__)              \

#define rpg_free(_p)                                                \
    _rpg_free(_p, __FILE__, __LINE__)                               \


void  *_rpg_alloc(size_t size, const char *name, int line);
void * _rpg_zalloc(size_t size, const char *name, int line);
void *_rpg_calloc(size_t nmemb, size_t size, const char *name, int line);
void *_rpg_realloc(void *ptr, size_t size, const char *name, int line);
void _rpg_free(void *ptr, const char *name, int line);


#define NOT_REACHED() rpg_assert("not reached", __FILE__, __LINE__);                                      
#define ASSERT(_x) do {                                             \
    if (!(_x)) {                                                    \
        rpg_assert(#_x, __FILE__, __LINE__);                        \
    }                                                               \
} while(0)

void rpg_assert(const char *cond, const char *file, int line);

#endif

