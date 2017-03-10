#include "core.h"
#include "server.h"
#include "_string.h"
#include "util.h"
#include "upstream.h"
#include "proto/s5.h"
#include "proto/http.h"


rps_status_t
server_init(struct server *s, struct config_server *cfg, struct upstreams *us) {
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

    s->proto = rps_proto_int((const char *)cfg->proto.data);

    if (s->proto < 0){
        log_error("unsupport proxy protocol type: %s", cfg->proto.data);
        return RPS_ERROR;
    }
     
    status = rps_resolve_inet((const char *)cfg->listen.data, cfg->port, &s->listen);
    if (status < 0) {
        log_error("resolve inet %s:%d failed", cfg->listen.data, cfg->port);
        return RPS_ERROR;
    }

    s->cfg = cfg;
    s->upstreams = us;

    return RPS_OK;
}


void
server_deinit(struct server *s) {
    uv_loop_close(&s->loop);

    /* Make valgrind happy */
    uv_loop_delete(&s->loop);
}

static void
server_sess_init(rps_sess_t *sess, struct server *s) {
    sess->server = s;
    sess->request = NULL;
    sess->forward = NULL;
}

static void
server_sess_free(rps_sess_t *sess) {
    if ((sess->request != NULL) && (sess->request->state & c_closed)) {
        rps_free(sess->request);
        sess->request = NULL;
    }

    if ((sess->forward != NULL) && (sess->forward->state & c_closed)) {
        rps_free(sess->forward);
        sess->forward = NULL;
    }

    if (sess->request != NULL || sess->forward != NULL) {
        return;
    }

    rps_free(sess);
}

static void
server_ctx_init(rps_ctx_t *ctx, rps_sess_t *sess, uint8_t flag, rps_proto_t proto) {
    ctx->sess = sess;
    ctx->flag = flag;
    ctx->state = c_init;
    ctx->proto = proto;
    ctx->nread = 0;
    ctx->last_status = 0;
    ctx->retry = 0;
    ctx->connected = 0;
    ctx->handle.handle.data  = ctx;
    ctx->write_req.data = ctx;
    ctx->timer.data = ctx;
    ctx->connect_req.data = ctx;

    if (ctx->proto == SOCKS5) {
        switch (ctx->flag) {
            case c_request:
                ctx->do_next = s5_server_do_next;
                break;
            case c_forward:
                ctx->do_next = s5_client_do_next;
                break;
            default:
                NOT_REACHED();
        }
#ifdef TODO_HTTP
    } else if (ctx->proto == HTTP) {
        switch (ctx->flag) {
            case c_request:
                ctx->do_next = http_server_do_next;
                break;
            case c_forward:
                ctx->do_next = http_client_do_next;
                break;
            default:
                NOT_REACHED();
        }
#endif
    } else {
        NOT_REACHED();
    }
    
/*
    if (ctx->flag == c_request) {
        switch (ctx->proto) {
            case SOCKS5:
                ctx->do_next = s5_server_do_next;
                break;
            case HTTP:
                ctx->do_next = http_server_do_next;
                break;
            #ifdef SOCKS4_PROXY_SUPPORT
            case SOCKS4:
                ctx->do_next = s4_server_do_next;
               break;
            #endif
            default:
               NOT_REACHED();
        }
    } else if (ctx->flag == c_forward) {
        switch (ctx->proto) {
            case SOCKS5:
                ctx->do_next = s5_client_do_next;
                break;
            case HTTP:
                ctx->do_next = http_client_do_next;
                break;
            #ifdef SOCKS4_PROXY_SUPPORT
            case SOCKS4:
                ctx->do_next = s4_client_do_next;
               break;
            #endif
            default:
               NOT_REACHED();
        }
    } else {
        NOT_REACHED();
    }
*/
}

static void 
server_on_ctx_close(uv_handle_t* handle) {
    //Set flag be closed and 
    rps_ctx_t *ctx;
    rps_sess_t *sess;

    ctx = handle->data;
    sess = ctx->sess;
    ctx->state = c_closed;
    ctx->connected = 0;

    switch (ctx->flag) {
        case c_request:
            log_debug("request from %s be closed", ctx->peername);
            break;
        case c_forward:
            log_debug("forward to %s be closed.", ctx->peername);
            break;
        default:
            NOT_REACHED();
    }

    server_do_next(ctx);
}

static void
server_ctx_close(rps_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->state & (c_closing | c_closed)) {
        return;
    }

    if (!ctx->connected) {
        return;
    }

    ctx->state = c_closing;
    uv_read_stop(&ctx->handle.stream);
    uv_timer_stop(&ctx->timer);
    uv_close(&ctx->handle.handle, (uv_close_cb)server_on_ctx_close);
}

static void
server_close(rps_sess_t *sess) {

    server_ctx_close(sess->request);
    server_ctx_close(sess->forward);
}


static uv_buf_t *
server_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    rps_ctx_t *ctx;

    UNUSED(suggested_size);

    ctx = handle->data;

    buf->base = ctx->buf;
    buf->len = sizeof(ctx->buf);

    return buf;
}

static void 
server_on_timer_expire(uv_timer_t *handle) {
    rps_ctx_t *ctx;

    ctx = handle->data;
    

    if (ctx->flag == c_request) {
        log_debug("request from %s timeout", ctx->peername);
    } else {
        log_debug("forward to %s timeout", ctx->peername);
    }

    ctx->state = c_kill;
    server_do_next(ctx);
}

static void 
server_timer_reset(rps_ctx_t *ctx) {
    int err;
    int timeout;

    timeout = ctx->sess->server->cfg->timeout;

    err = uv_timer_start(&ctx->timer, 
            (uv_timer_cb)server_on_timer_expire, timeout, 0);
    if (err) {
        UV_SHOW_ERROR(err, "reset timer");
    }
}


static void
server_on_read_done(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    rps_ctx_t *ctx;   
    
    ctx = stream->data;
    ASSERT(&ctx->handle.stream == stream);
    ASSERT(ctx->buf == buf->base);

    if (nread <0 ) {
        if (nread != UV_EOF) {
            UV_SHOW_ERROR(nread, "read error");
        }

        //Client close connect
        server_close(ctx->sess);
        return;

    }
    
    ctx->nread = nread;

#ifdef RPS_DEBUG_OPEN
    log_verb("read %zd bytes", ctx->nread);
    log_hex(LOG_VERBOSE, ctx->buf, ctx->nread);
#endif

    server_timer_reset(ctx);

    server_do_next(ctx);
}

rps_status_t
server_read(rps_ctx_t *ctx) {
    int err;

    err = uv_read_start(&ctx->handle.stream, 
            (uv_alloc_cb)server_alloc, (uv_read_cb)server_on_read_done);
    if (err < 0) {
        return RPS_ERROR;
    }
    
    return RPS_OK;
}

static void
server_on_write_done(uv_write_t *req, int err) {
    rps_ctx_t *ctx;

    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    ctx = req->data;
    
    ctx->last_status = err;

    if (err) {
        UV_SHOW_ERROR(err, "on write done");
        server_do_next(ctx);
        return;
    }
}

rps_status_t
server_write(rps_ctx_t *ctx, const void *data, size_t len) {
    int err;
    uv_buf_t buf;
    
    buf.base = (char *)data;
    buf.len = len;

    err = uv_write(&ctx->write_req, 
             &ctx->handle.stream, 
             &buf, 
             1, 
             server_on_write_done);

    if (err) {
        UV_SHOW_ERROR(err, "write");
        return RPS_ERROR;
    }

#if RPS_DEBUG_OPEN
    log_verb("write %zd bytes", len);
    log_hex(LOG_VERBOSE, (char *)data, len);
#endif

    server_timer_reset(ctx);
    
    return RPS_OK;
}

static void
server_on_connect_done(uv_connect_t *req, int err) {
    rps_ctx_t *ctx;

    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    if (err) {
        UV_SHOW_ERROR(err, "on connect done");
    }

    ctx = req->data;
    
    ctx->last_status = err;
    ctx->connected = 1;
    server_do_next(ctx);
}


static rps_status_t
server_connect(rps_ctx_t *ctx) {
    int err;

    err = uv_tcp_connect(&ctx->connect_req, 
            &ctx->handle.tcp, 
            (const struct sockaddr *)&ctx->peer.addr,
            server_on_connect_done);

    if (err) {
        UV_SHOW_ERROR(err, "tcp connect");
        return RPS_ERROR;
    }

    server_timer_reset(ctx);

#if RPS_DEBUG_OPEN
    log_verb("begin connect %s", ctx->peername);
#endif


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
server_on_request_connect(uv_stream_t *us, int err) {
    struct server *s;
    rps_sess_t *sess;
    rps_ctx_t *request; /* client -> rps */
    int len;
    rps_status_t status;

    if (err) {
        UV_SHOW_ERROR(err, "on new connect");
        return;
    }

    s = (struct server*)us->data;
    
    sess = (struct session*)rps_alloc(sizeof(struct session));
    if (sess == NULL) {
        return;
    }
    server_sess_init(sess, s);

    request = (struct context *)rps_alloc(sizeof(struct context));
    if (request == NULL) {
        return;
    }
    sess->request = request;
    server_ctx_init(request, sess, c_request, s->proto);
    
    uv_tcp_init(us->loop, &request->handle.tcp);
    uv_timer_init(us->loop, &request->timer);

    err = uv_accept(us, &request->handle.stream);
    if (err) {
        UV_SHOW_ERROR(err, "accept");
        goto error;
    }

    request->connected = 1;

    #ifdef REQUEST_TCP_KEEPALIVE
    err = uv_tcp_keepalive(&request->handle, 1, TCP_KEEPALIVE_DELAY);
    if (err) {
        UV_SHOW_ERROR(err, "set tcp keepalive");
        goto error;
    }
    #endif

    
    /*
     * Get client address info.
     */
    len = (int)s->listen.addrlen;
    err = uv_tcp_getpeername(&request->handle.tcp, 
            (struct sockaddr *)&sess->client.addr, &len);
    if (err) {
        UV_SHOW_ERROR(err, "getpeername");
        goto error;
    }
    sess->client.family = s->listen.family;
    sess->client.addrlen = len;

    memcpy(&request->peer, &sess->client, sizeof(sess->client));
    
    err = rps_unresolve_addr(&request->peer, request->peername);
    if (err < 0) {
        log_error("unresolve peername failer.");
        goto error;
    }

    log_debug("accept request from %s:%d", request->peername, rps_unresolve_port(&request->peer));

    request->state = c_handshake;

    /*
     * Beigin receive data
     */
    status = server_read(request);
    if (status != RPS_OK) {
        goto error;
    }

    return;

error:
    server_close(request->sess);
    return;
}

static void
server_upstream_kickoff(rps_ctx_t *ctx) {
    struct server *s;
    rps_sess_t *sess;
    rps_ctx_t *request;  /* client -> rps */
    rps_ctx_t *forward; /* rps -> upstream */

    sess = ctx->sess;
    s = sess->server;
    request = sess->request;

    
    forward = (struct context *)rps_alloc(sizeof(struct context));
    if (forward == NULL) {
        request->state = c_kill;
        server_do_next(request);
        return;
    }

    server_ctx_init(forward, sess, c_forward, request->proto);
    sess->forward = forward;
    
    uv_timer_init(&s->loop, &forward->timer);

    /*
     *  conext switch from reuqest to forward 
     */
    forward->state = c_conn;
    server_do_next(forward);
}

static void
server_upstream_connect(rps_ctx_t *ctx) {
    rps_sess_t *sess;
    struct server *s;
    rps_ctx_t   *forward;

    sess = ctx->sess;
    s = sess->server;
    forward = sess->forward;

    ASSERT(ctx == forward);
    ASSERT(ctx->state = c_conn);


    /* Be called after connect finished */
    if (ctx->retry > 0) {
        /* Last connect failed */
        if (ctx->last_status < 0) {
            log_warn("connect upstream %s:%d failed.", forward->peername, 
                    rps_unresolve_port(&forward->peer));
        } else {

            /* Connect success */
            log_debug("connect upstream %s://%s:%d ", rps_proto_str(forward->proto), forward->peername, 
                    rps_unresolve_port(&forward->peer));
            
            if (server_read(forward) != RPS_OK) {
                goto kill;
            }

            ctx->state = c_handshake;
            server_do_next(ctx);
            return;
        }

        if (ctx->retry >= s->upstreams->maxretry) {
            log_error("upstream connect failed after %d retry.", ctx->retry);
            goto kill;
        }

    }

    if (upstreams_get(s->upstreams, forward->proto, &sess->upstream) != RPS_OK) {
        log_error("no available %s upstream proxy.", rps_proto_str(forward->proto));
        goto kill;
    }

    /* upstream proto may be changed in hybrid mode */
    forward->proto = sess->upstream.proto;

    memcpy(&forward->peer, &sess->upstream.server, sizeof(sess->upstream.server));

    if (rps_unresolve_addr(&forward->peer, forward->peername) != RPS_OK) {;
        goto kill;
     }

    //uv_tcp_init must be called before call uv_tcp_connect each time.
    uv_tcp_init(&s->loop, &forward->handle.tcp);

    if (server_connect(forward) != RPS_OK) {
        log_warn("connect upstream %s:%d failed.", forward->peername, 
               rps_unresolve_port(&forward->peer));
        goto kill;
    }
    
    ctx->retry++;
    return;

kill:
    ctx->state = c_kill;
    server_do_next(ctx);
}


void
server_do_next(rps_ctx_t *ctx) {

    /* ignore connect error, we need retry */
    if (ctx->last_status < 0 && ctx->state != c_conn) {
        ctx->state = c_kill;
    }

    switch (ctx->state) {
        case c_reply_pre:
            server_upstream_kickoff(ctx);
            break;
        case c_conn:
            server_upstream_connect(ctx);
            break;
        case c_kill:
            server_close(ctx->sess);
            break;
        case c_closing:
            break;
        case c_closed:
            server_sess_free(ctx->sess);
            break;
        default:
            ctx->do_next(ctx);
            break;
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
    
    err = uv_listen((uv_stream_t*)&s->us, TCP_BACKLOG, server_on_request_connect);
    if (err) {
        UV_SHOW_ERROR(err, "listen");
        return;
    }

    log_notice("%s proxy run on %s:%d", s->cfg->proto.data, s->cfg->listen.data, s->cfg->port);

    uv_run(&s->loop, UV_RUN_DEFAULT);
}
