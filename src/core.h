#ifndef _RPG_CORE_H
#define _RPG_CORE_H

#include <sys/types.h>

#define RPG_OK      0
#define RPG_ERROR   -1

typedef int rpg_status_t;

struct application {
    int                 log_level;
    char                *log_filename;
    pid_t               pid;
    char                *pid_filename;
    char                *config_filename;
    struct config       *conf;
    unsigned            daemon:1;
    unsigned            verbose:1;
};


#endif
