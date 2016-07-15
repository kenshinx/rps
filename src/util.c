#include "util.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>


void *
_rpg_alloc(size_t size, const char *name, int line) {
    void *p;
    
    ASSERT(size != 0);

    p = malloc(size);
    
    if (p == NULL) {
        log_error("malloc(%zu) failed @ %s:%d", size, name, line);
    } else {
        log_verb("malloc(%zu) at %p @ %s:%d", size, p, name, line);
    }

    return p;
}

void *
_rpg_zalloc(size_t size, const char *name, int line) {
    void *p;

    p = _rpg_alloc(size, name, line);\
    if (p != NULL) {
        memset(p, 0, size);
    }

    return p;
}

void *
_rpg_calloc(size_t nmemb, size_t size, const char *name, int line) {
    return _rpg_alloc(nmemb * size, name, line);
}

void *
_rpg_realloc(void *ptr, size_t size, const char *name, int line) {
    void *p;
    
    ASSERT(size != 0);

    p = realloc(ptr, size);
    
    if (p == NULL) {
        log_error("realloc(%zu) failed @ %s:%d", size, name, line);
    } else {
        log_verb("realloc(%zu) at %p @ %s:%d", size, p, name, line);
    }
    
    return p;
}

void 
_rpg_free(void *ptr, const char *name, int line) {
    ASSERT(ptr != NULL);
    log_verb("free(%p) @ %s:%d", ptr, name, line);
    free(ptr);
}

void
rpg_assert(const char *cond, const char *file, int line) {
    log_error("assert '%s' failed @ (%s, %d)", cond, file, line);
    abort();
}
