#ifndef _RPG_SERVER_H
#define _RPG_SERVER_H

#include "core.h"
#include "config.h"

#include <uv.h>

struct server {
    uv_loop_t   *loop;   
};

rpg_status_t server_init(struct server *s, struct config_server *cs);


#endif
