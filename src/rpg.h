#ifndef _RPG_H
#define _RPG_H

#include "array.h"
#include "config.h"

#include <sys/types.h>

#define RPG_VERSION "0.1.2"

#ifdef RPG_DEBUG_OPEN
#define RPG_DEFAULT_LOG_LEVEL       LOG_VERBOSE
#else
#define RPG_DEFAULT_LOG_LEVEL       LOG_NOTICE
#endif
#define RPG_DEFAULT_LOG_FILE        NULL
#define RPG_DEFAULT_CONFIG_FILE     "../conf/rpg.yml"
#define RPG_DEFAULT_PID_FILE        NULL


struct application {
    rpg_array_t         servers;
    rpg_array_t    upstreams;

    int                 log_level;
    char                *log_filename;
    pid_t               pid;
    char                *pid_filename;
    char                *config_filename;
    struct config       *cfg;
    unsigned            daemon:1;
    unsigned            verbose:1;
};



#endif
