#ifndef _RPG_CONFIG_H
#define _RPG_CONFIG_H

#include <stdint.h>

#define CONFIG_DEFAULT_FILE     "../conf/rpg.yml"
#define CONFIG_DEFAULT_PIDFILE  NULL

struct config_server {
    char        *listen;
    uint16_t     port;
};


struct config {
    struct config_server **servers;
};


struct config *config_create(const char *config_file);

#endif
