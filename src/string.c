#include "core.h"
#include "string.h"
#include "util.h"

#include <stdint.h>
#include <string.h>

rps_str_t *
string_new() {
    rps_str_t *str;
    
    str = rps_alloc(sizeof(*str));
    if (str == NULL) {
        return NULL;
    }
    
    string_init(str);
    
    return str;
}

void 
string_free(rps_str_t *str) {
    string_deinit(str);
    rps_free(str);
}

rps_status_t
string_duplicate(rps_str_t *dst, const char *src, size_t len) {
    ASSERT(dst->data == NULL && dst->len == 0);
    ASSERT(src != NULL && len != 0 );
    
    dst->data = (uint8_t *)strndup(src, len + 1);
    if (dst->data == NULL) {
        return RPS_ENOMEM;
    }

    dst->len = len;
    dst->data[len] = '\0';
    
    return RPS_OK;
}


rps_status_t
string_copy(rps_str_t *dst, const rps_str_t *src) {
    ASSERT(dst->data == NULL && dst->len == 0);
    ASSERT(src->data != NULL && src->len != 0);

    dst->data = (uint8_t *)rps_alloc(src->len + 1);
    if (dst->data == NULL) {
        return RPS_ENOMEM;
    }

    memcpy(dst->data, src->data, src->len);
    dst->len = src->len;
    dst->data[dst->len] = '\0';
    
    return RPS_OK;
}
