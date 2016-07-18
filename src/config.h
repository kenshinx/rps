#ifndef _RPG_CONFIG_H
#define _RPG_CONFIG_H

#include "array.h"
#include "string.h"

#include <stdio.h>
#include <stdint.h>

struct config_server {
    rpg_str_t       *listen;
    uint16_t        port;
};

struct config_log {
    rpg_str_t       file;
    rpg_str_t       level;
};


struct config {
    char            *fname;
    FILE            *fd;
    rpg_str_t       title;
    unsigned        daemon:1;
    rpg_str_t       pidfile;

    rpg_array_t     *servers;
};


struct config *config_create(char *config_file);

#endif
