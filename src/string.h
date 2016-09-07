#ifndef _PRG_STRING_H
#define _PRG_STRING_H

#include "core.h"
#include "util.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct rpg_string {
    size_t      len;
    uint8_t     *data;
} rpg_str_t;

#define rpg_string(_str)    { sizeof(_str) - 1, (uint8_t *)(_str) }

static inline void
string_init(rpg_str_t *str) {
    str->len = 0;
    str->data = NULL;
}

static inline void
string_deinit(rpg_str_t *str) {
    if (str->data != NULL) {
        rpg_free(str->data);
        string_init(str);
    }
}

rpg_str_t *string_new();
void string_free(rpg_str_t *str);
rpg_status_t string_duplicate(rpg_str_t *dst, const char *src, size_t len);
rpg_status_t string_copy(rpg_str_t *dst, const rpg_str_t *src);



#define rpg_strcmp(_s1, _s2)        \
    strcmp((char *)(_s1), (char *)(_s2))

#endif

