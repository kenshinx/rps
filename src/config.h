#ifndef _RPG_CONFIG_H
#define _RPG_CONFIG_H

#include "array.h"
#include "string.h"

#include <yaml.h>

#include <stdio.h>
#include <stdint.h>

struct config_server {
    rpg_str_t       listen;
    uint16_t        port;
};

struct config_log {
    rpg_str_t       file;
    rpg_str_t       level;
};


struct config {
    char            *fname;
    rpg_str_t       title;
    rpg_str_t       pidfile;
    FILE            *fd;
    unsigned        daemon:1;
    rpg_array_t     *servers;
    rpg_array_t     *args;
    uint32_t        depth;
    unsigned        seq:1;
    yaml_parser_t   parser;
    yaml_event_t    event;
    yaml_token_t    token;
    
};


struct config *config_create(char *config_file);
void config_destroy(struct config *cfg);

#endif
