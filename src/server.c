#include "server.h"
#include "core.h"
#include "string.h"
#include "util.h"


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

    s->us.data = s;

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
        UV_SHOW_ERROR(err, "ip4 addr");
        return RPS_ERROR;
    }

    s->cfg = cfg;

    return RPS_OK;
}


void
server_deinit(struct server *s) {
    uv_loop_close(&s->loop);
}

static rps_status_t 
server_context_init(rps_ctx_t *ctx) {
    return RPS_OK;
}


/*
 *         request            forward
 * Client  ------->    RPS  ----------> Upstream ----> Remote
 *         session            session
 *  |                                      |
 *  |  ---          context          ---   |
 */

static void
server_on_new_connect(uv_stream_t *us, int err) {
    struct server *s;
    rps_ctx_t *ctx;
    rps_sess_t *request; /* client -> rps */
    rps_sess_t *forward; /* rps -> upstream */
    rps_status_t status;

    if (err) {
        UV_SHOW_ERROR(err, "on new connect");
        return;
    }

    s = (struct server*)us->data;
    
    ctx = (struct context*)rps_alloc(sizeof(struct context));
    if (ctx == NULL) {
        return;
    }

    status = server_context_init(ctx);
    if (status != RPS_OK) {
        rps_free(ctx);
        return;
    }

    request =  &ctx->request;

    uv_tcp_init(us->loop, &request->handler);
    uv_timer_init(us->loop, &request->timer);

    err = uv_accept(us, (uv_stream_t *)&request->handler);
    if (err) {
        UV_SHOW_ERROR(err, "accept");
        uv_close((uv_handle_t *)&request->handler, NULL);
        rps_free(ctx);
    }

    #ifdef REQUEST_TCP_KEEPALIVE
    err = uv_tcp_keepalive(&request->handle, 1, TCP_KEEPALIVE_DELAY);
    if (err) {
        UV_SHOW_ERROR(err, "set tcp keepalive");
        uv_close((uv_handle_t *)&request->handler, NULL);
        rps_free(ctx);
    }
    #endif
    
}

void 
server_run(struct server *s) {
    int err;

    err = uv_tcp_bind(&s->us, &s->listen, 0);
    if (err) {
        UV_SHOW_ERROR(err, "bind");
        return;
    }
    
    err = uv_listen((uv_stream_t*)&s->us, HTTP_DEFAULT_BACKLOG, server_on_new_connect);
    if (err) {
        UV_SHOW_ERROR(err, "listen");
        return;
    }

    log_notice("%s proxy run on %s:%d", s->cfg->proxy.data, s->cfg->listen.data, s->cfg->port);

    uv_run(&s->loop, UV_RUN_DEFAULT);
}