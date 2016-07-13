#ifndef _RPG_CORE_H
#define _RPG_CORE_H

#include <sys/types.h>

#define RPG_OK      0
#define RPG_ERROR   -1

typedef int rpg_status_t;

struct application {
    char        *conf_file;
    pid_t       pid;
    char        *pid_file;
    unsigned    daemon:1;
};


#endif
