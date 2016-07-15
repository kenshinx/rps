#ifndef _RPG_CONFIG_H
#define _RPG_CONFIG_H

#include "array.h"

#include <stdio.h>
#include <stdint.h>

struct config_server {
    char        *listen;
    uint16_t     port;
};


struct config {
    char            *fname;
    FILE            *fd;
    rpg_array_t     *servers;
};


struct config *config_create(char *config_file);

#endif
