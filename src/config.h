#ifndef _RPS_CONFIG_H
#define _RPS_CONFIG_H

#include "core.h"
#include "array.h"
#include "_string.h"

#include <yaml.h>

#include <stdio.h>
#include <stdint.h>

struct config_server {
    rps_str_t       proxy;
    rps_str_t       listen;
    uint16_t        port;
    rps_str_t       username;
    rps_str_t       password;
    uint32_t        timeout;
};

struct config_redis {
    rps_str_t   host;
    uint16_t    port;
    uint16_t    db;
    rps_str_t   password;
    uint32_t    timeout;
};

struct config_log {
    rps_str_t       file;
    rps_str_t       level;
};


struct config {
    char                *fname;
    rps_str_t           title;
    rps_str_t           pidfile;
    FILE                *fd;
    unsigned            daemon:1;
    struct config_redis *redis;
    struct config_log   *log;
    rps_array_t         *servers;
    rps_array_t         *args;
    uint32_t            depth;
    unsigned            seq:1;
    yaml_parser_t       parser;
    yaml_event_t        event;
    
};


rps_status_t config_init(char *config_file, struct config *cfg);
void config_deinit(struct config *cfg);
void config_dump(struct config *cfg);

#endif
