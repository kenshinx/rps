#ifndef _RPG_SERVER_H
#define _RPG_SERVER_H

#include "core.h"
#include "string.h"
#include "config.h"

#include <uv.h>

#include <unistd.h>

struct server {
    uv_loop_t       loop;   
    uv_tcp_t        us; /* libuv tcp server */

    rpg_proxy_t     proxy;

    struct listen {
        rpg_str_t           host;
        uint16_t            port;
        struct sockaddr     addr;
    };        
    
};

rpg_status_t server_init(struct server *s, struct config_server *cs);
void server_deinit(struct server *s);

/*
 * server_init
 * server_deinit
 * server_run
 * server_stop
 *
 */

#endif
