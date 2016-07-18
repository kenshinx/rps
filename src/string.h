#ifndef _PRG_STRING_H
#define _PRG_STRING_H

typedef struct rpg_string {
    size_t      len;
    uint8_t     *data;
} rpg_str_t;

#define rpg_string(_str)    { sizeof(_str) - 1, (uint8_t *)(_str) }

#endif

