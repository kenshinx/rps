#ifndef _PRS_STRING_H
#define _PRS_STRING_H

#include "util.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct rps_string {
    size_t      len;
    uint8_t     *data;
} rps_str_t;

#define rps_string(_str)    { sizeof(_str) - 1, (uint8_t *)(_str) }

static inline void
string_init(rps_str_t *str) {
    str->len = 0;
    str->data = NULL;
}

static inline void
string_deinit(rps_str_t *str) {
    if (str->data != NULL) {
        rps_free(str->data);
        string_init(str);
    }
}

rps_str_t *string_new();
void string_free(rps_str_t *str);
int string_duplicate(rps_str_t *dst, const char *src, size_t len);
int string_copy(rps_str_t *dst, const rps_str_t *src);



#define rps_strcmp(_s1, _s2)        \
    strcmp((char *)(_s1), (char *)(_s2))

#endif

