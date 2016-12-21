#include "server.h"
#include "core.h"
#include "string.h"
#include "util.h"

#define HTTP_DEFAULT_BACKLOG  65536

rps_status_t
server_init(struct server *s, struct config_server *cfg) {
    uv_tcp_t *us;
    int err;

    err = uv_loop_init(&s->loop);
    if (err != 0) {
        UV_SHOW_ERROR(err, "loop init");
        return RPS_ERROR;
    }

    err = uv_tcp_init(&s->loop, &s->us);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "tcp init");
        return RPS_ERROR;
    }

    if (rps_strcmp(cfg->proxy.data, "socks5") == 0 ) {
        s->proxy = SOCKS5;
    } else if (rps_strcmp(cfg->proxy.data, "http") == 0 ) {
        s->proxy = HTTP;
    }
#ifdef SOCKS4_PROXY_SUPPORT
     else if (rps_strcmp(cfg->proxy.data, "socks4") == 0 ) {
        s->proxy = SOCKS4;
     }
#endif
    else{

        log_error("unsupport proxy type: %s", cfg->proxy.data);
        return RPS_ERROR;
    }
    
    err = uv_ip4_addr((const char *)cfg->listen.data, cfg->port, (struct sockaddr_in *)&s->listen);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "uv ip4 addr");
        return RPS_ERROR;
    }

    s->cfg = cfg;

    return RPS_OK;
}


void
server_deinit(struct server *s) {
}

static void
server_on_new_connect(uv_stream_t *server, int status) {

}

void 
server_run(struct server *s) {
    int err;

    err = uv_tcp_bind(&s->us, &s->listen, 0);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "uv bind");
        return;
    }
    
    err = uv_listen((uv_stream_t*)&s->us, HTTP_DEFAULT_BACKLOG, server_on_new_connect);
    if (err) {
        UV_SHOW_ERROR(err, "uv listen");
        return;
    }

    log_notice("%s proxy run on %s:%d", s->cfg->proxy.data, s->cfg->listen.data, s->cfg->port);

    uv_run(&s->loop, UV_RUN_DEFAULT);
}
