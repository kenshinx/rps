#include "server.h"
#include "core.h"
#include "string.h"
#include "util.h"


rps_status_t
server_init(struct server *s, struct config_server *cfg) {
    uv_tcp_t *us;
    int err;
    int status;

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
     
    status = rps_resolve_inet((const char *)cfg->listen.data, cfg->port, &s->listen);
    if (status < 0) {
        log_error("resolve inet %s:%d failed", cfg->listen.data, cfg->port);
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
server_session_init(rps_sess_t *sess) {
    return RPS_OK;
}


/*
 *         request            forward
 * Client  ------->    RPS  ----------> Upstream ----> Remote
 *         context            context
 *  |                                      |
 *  |  ---          session          ---   |
 */

static void
server_on_new_connect(uv_stream_t *us, int err) {
    struct server *s;
    rps_sess_t *sess;
    rps_ctx_t *request; /* client -> rps */
    rps_ctx_t *forward; /* rps -> upstream */
    rps_status_t status;
    int len;
    char *clientip;

    if (err) {
        UV_SHOW_ERROR(err, "on new connect");
        return;
    }

    s = (struct server*)us->data;
    
    sess = (struct session*)rps_alloc(sizeof(struct session));
    if (sess == NULL) {
        return;
    }

    status = server_session_init(sess);
    if (status != RPS_OK) {
        rps_free(sess);
        return;
    }

    request =  &sess->request;

    uv_tcp_init(us->loop, &request->handler);
    uv_timer_init(us->loop, &request->timer);

    err = uv_accept(us, (uv_stream_t *)&request->handler);
    if (err) {
        UV_SHOW_ERROR(err, "accept");
        uv_close((uv_handle_t *)&request->handler, NULL);
        rps_free(sess);
    }

    #ifdef REQUEST_TCP_KEEPALIVE
    err = uv_tcp_keepalive(&request->handle, 1, TCP_KEEPALIVE_DELAY);
    if (err) {
        UV_SHOW_ERROR(err, "set tcp keepalive");
        uv_close((uv_handle_t *)&request->handler, NULL);
        rps_free(sess);
    }
    #endif

    
    /*
     * Get client address info.
     */
    
    len = (int)s->listen.addrlen;
    err = uv_tcp_getpeername(&request->handler, (struct sockaddr *)&sess->client.addr, &len);
    if (err) {
        UV_SHOW_ERROR(err, "getpeername");
        uv_close((uv_handle_t *)&request->handler, NULL);
        rps_free(sess);
    }
    sess->client.family = s->listen.family;
    sess->client.addrlen = len;
    
    clientip = rps_unresolve_addr(&sess->client);
    if (clientip != NULL) {
        printf("%s\n", clientip);
        rps_free(clientip);
    }
    
}

void 
server_run(struct server *s) {
    int err;

    err = uv_tcp_bind(&s->us, (struct sockaddr *)&s->listen.addr, 0);
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
