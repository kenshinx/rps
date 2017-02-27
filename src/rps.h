#ifndef _RPS_H
#define _RPS_H

#include "array.h"
#include "config.h"
#include "proxy.h"

#include <sys/types.h>

#define RPS_VERSION "0.1.2"

#ifdef RPS_DEBUG_OPEN
#define RPS_DEFAULT_LOG_LEVEL       LOG_VERBOSE
#else
#define RPS_DEFAULT_LOG_LEVEL       LOG_NOTICE
#endif
#define RPS_DEFAULT_LOG_FILE        NULL
#define RPS_DEFAULT_CONFIG_FILE     "../conf/rps.yml"
#define RPS_DEFAULT_PID_FILE        NULL


struct application {
    rps_array_t         servers;

    struct proxy_pool   upstreams;

    int                 log_level;
    char                *log_filename;
    pid_t               pid;
    char                *pid_filename;
    char                *config_filename;
    struct config       cfg;
    unsigned            daemon:1;
    unsigned            verbose:1;
};



#endif
