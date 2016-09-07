#include "server.h"
#include "core.h"
#include "string.h"
#include "util.h"

rpg_status_t
server_init(struct server *s, struct config_server *cfg) {
    uv_tcp_t *us;
    int err;
    rpg_status_t status;

    err = uv_loop_init(&s->loop);
    if (err != 0) {
        UV_SHOW_ERROR(err, "loop init");
        return RPG_ERROR;
    }

    err = uv_tcp_init(&s->loop, &s->us);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "tcp init");
        return RPG_ERROR;
    }

    if (rpg_strcmp(cfg->proxy.data, "socks5") == 0 ) {
        s->proxy = SOCKS5;
    } else if (rpg_strcmp(cfg->proxy.data, "http") == 0 ) {
        s->proxy = HTTP;
    }
#ifdef SOCKS4_PROXY_SUPPORT
     else if (rpg_strcmp(cfg->proxy.data, "socks4") == 0 ) {
        s->proxy = SOCKS4;
     }
#endif
    else{

        log_error("unsupport proxy type: %s", cfg->proxy.data);
        return RPG_ERROR;
    }
    
    err = uv_ip4_addr((const char *)cfg->listen.data, cfg->port, (struct sockaddr_in *)&s->listen);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "uv ip4 addr");
        return RPG_ERROR;
    }

    s->cfg = cfg;

    return RPG_OK;
}


void
server_deinit(struct server *s) {
    log_notice("%s proxy run on %s:%d", s->cfg->proxy.data, s->cfg->listen.data, s->cfg->port);
}

void 
server_run(struct server *s) {
}
