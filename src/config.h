#ifndef _RPS_CONFIG_H
#define _RPS_CONFIG_H

#include "array.h"
#include "_string.h"

#include <yaml.h>

#include <stdio.h>
#include <stdint.h>

#define UPSTREAM_DEFAULT_REFRESH    60
#define UPSTREAM_DEFAULT_BYBRID     0
#define UPSTREAM_DEFAULT_MAXRECONN   3
#define UPSTREAM_DEFAULT_MAXRETRY   3

struct config_servers {
    rps_array_t     *ss;
    uint32_t        rtimeout;
    uint32_t        ftimeout;
};

struct config_server {
    rps_str_t       proto;
    rps_str_t       listen;
    uint16_t        port;
    rps_str_t       username;
    rps_str_t       password;
};

struct config_upstream {
    rps_str_t       proto;
    rps_str_t       rediskey;
};

struct config_upstreams {
    uint32_t        refresh;
    rps_str_t       schedule;
    unsigned        hybrid:1;
    uint32_t        maxreconn;
    uint32_t        maxretry;
    rps_array_t     *pools;
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
    char                    *fname;
    rps_str_t               title;
    rps_str_t               pidfile;
    FILE                    *fd;
    unsigned                daemon:1;
    struct config_servers   servers;
    struct config_upstreams upstreams;
    struct config_redis     redis;
    struct config_log       log;
    rps_array_t             *args;
    uint32_t                depth;
    unsigned                seq:1;
    yaml_parser_t           parser;
    yaml_event_t            event;
    
};


int config_init(char *config_file, struct config *cfg);
void config_deinit(struct config *cfg);
void config_dump(struct config *cfg);

#endif
