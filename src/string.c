#include "core.h"
#include "string.h"
#include "util.h"

#include <stdint.h>
#include <string.h>

rpg_str_t *
string_new() {
    rpg_str_t *str;
    
    str = rpg_alloc(sizeof(*str));
    if (str == NULL) {
        return NULL;
    }
    
    string_init(str);
    
    return str;
}

void 
string_free(rpg_str_t *str) {
    string_deinit(str);
    rpg_free(str);
}

rpg_status_t
string_dup(rpg_str_t *dst, const char *src, size_t len) {
    ASSERT(dst->data == NULL && dst->len == 0);
    ASSERT(src != NULL && len != 0 );
    
    dst->data = (uint8_t *)strndup(src, len + 1);
    if (dst->data == NULL) {
        return RPG_ENOMEM;
    }

    dst->len = len;
    dst->data[len] = '\0';
    
    return RPG_OK;
}


rpg_status_t
string_cpy(rpg_str_t *dst, const rpg_str_t *src) {
    ASSERT(dst->data == NULL && dst->len == 0);
    ASSERT(src->data != NULL && src->len != 0);

    dst->data = (uint8_t *)rpg_alloc(src->len + 1);
    if (dst->data == NULL) {
        return RPG_ENOMEM;
    }

    memcpy(dst->data, src->data, src->len);
    dst->len = src->len;
    dst->data[dst->len] = '\0';
    
    return RPG_OK;
}
