#include "core.h"
#include "server.h"
#include "_string.h"
#include "util.h"
#include "upstream.h"
#include "proto/s5.h"
#include "proto/http_proxy.h"
#include "proto/http_tunnel.h"


rps_status_t
server_init(struct server *s, struct config_server *cfg, 
        struct upstreams *us, uint32_t rtimeout, uint32_t ftimeout) {
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
    s->rtimeout = rtimeout;
    s->ftimeout = ftimeout;

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
    upstream_init(&sess->upstream);
}

static void
server_sess_free(rps_sess_t *sess) {
    if (((sess->request != NULL)) && (sess->request->state & c_closed)) {
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

    upstream_deinit(&sess->upstream);
    rps_free(sess);
}

static rps_status_t
server_ctx_init(rps_ctx_t *ctx, rps_sess_t *sess, uint8_t flag, uint32_t timeout) {
    ctx->sess = sess;
    ctx->flag = flag;
    ctx->state = c_init;
    ctx->nread = 0;
    ctx->nwrite = 0;
    ctx->nwrite2 = 0;
    ctx->reconn = 0;
    ctx->retry = 0;
    ctx->connecting = 0;
    ctx->connected = 0;
    ctx->established = 0;
    ctx->reply_code = rps_rep_undefined;
    ctx->rstat = c_stop;
    ctx->wstat = c_stop;
    ctx->timeout = timeout;
    ctx->handle.handle.data  = ctx;
    ctx->write_req.data = ctx;
    ctx->timer.data = ctx;
    ctx->connect_req.data = ctx;
    ctx->shutdown_req.data = ctx;

    ctx->wbuf = (char *)rps_alloc(WRITE_BUF_SIZE);
    if (ctx->wbuf == NULL) {
        return RPS_ENOMEM;
    }
    
    ctx->wbuf2 = (char *)rps_alloc(WRITE_BUF_SIZE);
    if (ctx->wbuf2 == NULL) {
        return RPS_ENOMEM;
    }

    ctx->req = NULL;

    return RPS_OK;
}

static void 
server_ctx_set_proto(rps_ctx_t *ctx, rps_proto_t proto) {

    ctx->proto = proto;

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
    } else if (ctx->proto == HTTP) {
        switch (ctx->flag) {
            case c_request:
                ctx->do_next = http_proxy_server_do_next;
                break;
            case c_forward:
                ctx->do_next = http_proxy_client_do_next;
                break;
            default:
                NOT_REACHED();
        }
    } else if (ctx->proto == HTTP_TUNNEL) {
        switch (ctx->flag) {
            case c_request:
                ctx->do_next = http_tunnel_server_do_next;
                break;
            case c_forward:
                ctx->do_next = http_tunnel_client_do_next;
                break;
            default:
                NOT_REACHED();
        }
    } else {
        NOT_REACHED();
    }
}


static void
server_ctx_deinit(rps_ctx_t *ctx) {

    ASSERT(ctx != NULL);

    ctx->state = c_closed;
    ctx->connecting = 0;
    ctx->connected = 0;
    ctx->established = 0;

    ctx->handle.handle.data  = NULL;
    ctx->write_req.data = NULL;
    ctx->timer.data = NULL;
    ctx->connect_req.data = NULL;
    ctx->shutdown_req.data = NULL;

    rps_free(ctx->wbuf);
    rps_free(ctx->wbuf2);

    if (ctx->req != NULL) {
        rps_free(ctx->req);
    }
}


static bool
server_ctx_dead(rps_ctx_t *ctx) {

    if (ctx == NULL) {
        return true;
    }

    if (ctx->state & (c_closing | c_closed)) {
        return true;
    }

    return false;
    
}


static void 
server_on_ctx_close(uv_handle_t* handle) {
    //Set flag be closed and 
    rps_ctx_t *ctx;

    ctx = handle->data;
    
    server_ctx_deinit(ctx);

    switch (ctx->flag) {
        case c_request:
            log_debug("Request from %s be closed", ctx->peername);
            break;
        case c_forward:
            log_debug("Forward to %s be closed.", ctx->peername);
            break;
        default:
            NOT_REACHED();
    }

    server_do_next(ctx);
}



static void
server_ctx_close(rps_ctx_t *ctx) {

    if (server_ctx_dead(ctx)) {
        return;
    }

    uv_timer_stop(&ctx->timer);

    if (!ctx->connecting && !ctx->connected) {
        // we still need free context and session 
        // even if connect didn't established 
        server_ctx_deinit(ctx);
        server_do_next(ctx);
        return;
    }

    //The last chance for context to recycle the allocated resources.
    ctx->state = c_closing;
    server_do_next(ctx);

    uv_read_stop(&ctx->handle.stream);
    uv_close(&ctx->handle.handle, (uv_close_cb)server_on_ctx_close);
}

static void
server_close(rps_sess_t *sess) {

    server_ctx_close(sess->request);
    server_ctx_close(sess->forward);
}

static void
server_on_ctx_shutdown(uv_shutdown_t* req, int err) {
    
    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    if (err) {
        UV_SHOW_ERROR(err, "on shutdown done");
        return;
    }

    server_ctx_close((rps_ctx_t *)req->data);
}

static void
server_ctx_shutdown(rps_ctx_t *ctx) {
    /* uv_shutdown can ensure all the write-queue data has been sent out before close handle */
    int err;

    if (server_ctx_dead(ctx)) {
        return;
    }
    
    err = uv_shutdown(&ctx->shutdown_req, &ctx->handle.stream, server_on_ctx_shutdown);
    if (err) {
        UV_SHOW_ERROR(err, "shutdown");
    }
}


static uv_buf_t *
server_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    rps_ctx_t *ctx;

    UNUSED(suggested_size);

    ctx = handle->data;

    buf->base = ctx->rbuf;
    buf->len = sizeof(ctx->rbuf);

    return buf;
}

static void 
server_on_timer_expire(uv_timer_t *handle) {
    rps_ctx_t *ctx;

    ctx = handle->data;

    if (ctx == NULL) {
        return;
    }
    

    if (ctx->flag == c_request) {
        ctx->state = c_kill;
        log_debug("Request from %s timeout", ctx->peername);
    } else {
        /* bidirectional handshake has been established retry dosen’t make sense */
        if (ctx->established) {
            ctx->state = c_kill;
        } else {
            ctx->state = c_retry;
        }
        log_debug("Forward to %s timeout", ctx->peername);
    }

    server_do_next(ctx);
}

static void 
server_timer_reset(rps_ctx_t *ctx) {
    int err;

    err = uv_timer_start(&ctx->timer, 
            (uv_timer_cb)server_on_timer_expire, ctx->timeout, 0);
    if (err) {
        char why[256];
        snprintf(why, 256, "reset timer %s", ctx->peername);
        UV_SHOW_ERROR(err, why);
    }
}


static void
server_on_read_done(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    rps_ctx_t *ctx;   
    
    ctx = stream->data;

    if (ctx == NULL) {
        return;
    }

    ASSERT(&ctx->handle.stream == stream);
    ASSERT(ctx->rbuf == buf->base);

    ctx->rstat = c_done;
    ctx->nread = nread;

    if (nread <0 ) {
        
        if (ctx->state & (c_established | c_pipelined)) {
            // May be read error or EOF
            server_do_next(ctx);
            return;
        }

        if (ctx->flag == c_forward) {
            ctx->state = c_retry;
            server_do_next(ctx);
            return;
        }

        ctx->state = c_kill;
        server_do_next(ctx);
        return;

    }

    /* nread equal 0 is equivalent to EAGAIN or EWOULDBLOCK */
    if (nread == 0) {
        return;
    }


#ifdef RPS_DEBUG_OPEN
    if (ctx->proto == SOCKS5 && ctx->state < c_established) {
        log_verb("read %zd bytes", ctx->nread);
        log_hex(LOG_VERBOSE, ctx->rbuf, ctx->nread);
    }
#endif

    server_timer_reset(ctx);

    server_do_next(ctx);
}

static rps_status_t
server_read_start(rps_ctx_t *ctx) {
    int err;

    err = uv_read_start(&ctx->handle.stream, 
            (uv_alloc_cb)server_alloc, (uv_read_cb)server_on_read_done);
    if (err < 0) {
        return RPS_ERROR;
    }
    
    ctx->rstat = c_busy;

    return RPS_OK;
}

static void
server_read_stop(rps_ctx_t *ctx) {
    uv_read_stop(&ctx->handle.stream);
    ctx->rstat = c_stop;
}

static void
server_on_write_done(uv_write_t *req, int err) {
    rps_ctx_t *ctx;

    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    ctx = req->data;

    if (ctx == NULL) {
        return;
    }

    ctx->wstat = c_done;

    if (err) {
        char why[256];
        snprintf(why, 256, "on write to %s", ctx->peername);
        UV_SHOW_ERROR(err, why);
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

    if (ctx->nwrite2 > 0) {
        server_write(ctx, ctx->wbuf2, ctx->nwrite2);
        ctx->nwrite2 = 0;
    }

}

rps_status_t
server_write(rps_ctx_t *ctx, const void *data, size_t len) {
    int err;
    uv_buf_t buf;
    size_t slot;

    ASSERT(len > 0);

    if (ctx->wstat == c_busy) {
        slot = WRITE_BUF_SIZE - ctx->nwrite2;
        if (slot == 0) {
            log_debug("write buffer to %s has been full, drop %d bytes.", ctx->peername, len);
            return RPS_OK; 
        }

        len = MIN(slot, len);

        memcpy(&ctx->wbuf2[ctx->nwrite2], data, len);
        ctx->nwrite2 += len;
        return RPS_OK;
    }


    memcpy(ctx->wbuf, data, len);
    ctx->nwrite = len;

    buf.base = (char *)ctx->wbuf;
    buf.len = len;

    
    err = uv_write(&ctx->write_req, 
             &ctx->handle.stream, 
             &buf, 
             1, 
             server_on_write_done);

    if (err) {
        char why[256];
        snprintf(why, 256, "write to %s", ctx->peername);
        UV_SHOW_ERROR(err, why);
        return RPS_ERROR;
    }
    

#if RPS_DEBUG_OPEN
    if (ctx->proto == SOCKS5 && ctx->state < c_established) {
        log_verb("write %zd bytes", len);
        log_hex(LOG_VERBOSE, (char *)data, len);
    }
#endif

    ctx->wstat = c_busy;

    server_timer_reset(ctx);
    
    return RPS_OK;
}

static void
server_on_connect_done(uv_connect_t *req, int err) {
    rps_ctx_t *ctx;

    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    ctx = req->data;

    if (ctx == NULL) {
        return;
    }

    if (err) {
        char why[256];
        snprintf(why, 256, "on connect with %s", ctx->peername);
        UV_SHOW_ERROR(err, why);
        ctx->connected = 0;
    } else {
        ctx->connected = 1;
    }

    ctx->connecting = 0;

    /* request maybe killed before forward connected. */
    if (ctx->flag == c_forward && server_ctx_dead(ctx->sess->request)) {
        ctx->state = c_kill;
    }

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
        char why[256];
        snprintf(why, 256, "tcp connect %s", ctx->peername);
        UV_SHOW_ERROR(err, why);
        return RPS_ERROR;
    }

    ctx->connecting = 1;

    server_timer_reset(ctx);

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
    status = server_ctx_init(request, sess, c_request, s->rtimeout);
    if (status != RPS_OK) {
        return;
    }

    server_ctx_set_proto(request, s->proto);
    
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
            (struct sockaddr *)&request->peer.addr, &len);
    if (err) {
        UV_SHOW_ERROR(err, "getpeername");
        goto error;
    }
    request->peer.family = s->listen.family;
    request->peer.addrlen = len;
    
    err = rps_unresolve_addr(&request->peer, request->peername);
    if (err < 0) {
        log_error("unresolve peername failer.");
        goto error;
    }

    log_debug("Accept request from %s:%d", request->peername, rps_unresolve_port(&request->peer));

    request->state = c_handshake_req;

    /*
     * Beigin receive data
     */
    status = server_read_start(request);
    if (status != RPS_OK) {
        goto error;
    }

    return;

error:
    server_close(request->sess);
    return;
}

static void
server_switch(rps_sess_t *sess) {
    struct server *s;
    rps_ctx_t *request;  /* client -> rps */
    rps_ctx_t *forward; /* rps -> upstream */

    ASSERT(sess->forward == NULL);

    s = sess->server;
    request = sess->request;
    /* request stop read, wait for upstream establishment finished */
    server_read_stop(request);

    forward = (struct context *)rps_alloc(sizeof(struct context));
    if (forward == NULL) {
        request->state = c_kill;
        server_do_next(request);
        return;
    }

    if (server_ctx_init(forward, sess, c_forward,  s->ftimeout) != RPS_OK) {
        request->state = c_kill;
        server_do_next(request);
        return;
    }
    sess->forward = forward;
    
    uv_timer_init(&s->loop, &forward->timer);

    /*
     *  conext switch from reuqest to forward 
     */
    forward->state = c_conn;
    server_do_next(forward); 
}

static void
server_forward_connect(rps_sess_t *sess) {
    struct server *s;
    rps_ctx_t   *forward;

    s = sess->server;
    forward = sess->forward;

    ASSERT(forward->state = c_conn);


    /* Be called after connect finished */
    if (forward->reconn > 0) {
        /* Last connect failed */
        if (!forward->connected) {
            log_debug("Connect upstream %s:%d failed. reconn: %d", forward->peername, 
                    rps_unresolve_port(&forward->peer), forward->reconn);
        } else {

            server_ctx_set_proto(forward, sess->upstream.proto);

            /* Connect success */
            log_debug("Connect upstream %s://%s:%d success", rps_proto_str(forward->proto), forward->peername, 
                    rps_unresolve_port(&forward->peer));
            
            if (server_read_start(forward) != RPS_OK) {
                goto kill;
            }

            /* Set forward protocol after upstream has connected */
            forward->reconn = 0;
            forward->state = c_handshake_req;
            server_do_next(forward);
            return;
        }

        if (forward->reconn >= s->upstreams->maxreconn) {
            log_error("Connect upstream failed after %d reconn.", forward->reconn);
            forward->state = c_retry;
            server_do_next(forward);
            return;
        }

    }

    upstream_deinit(&sess->upstream);

    if (upstreams_get(s->upstreams, sess->request->proto, &sess->upstream) != RPS_OK) {
        log_error("no available %s upstream proxy.", rps_proto_str(sess->request->proto));
        goto kill;
    }


    memcpy(&forward->peer, &sess->upstream.server, sizeof(sess->upstream.server));

    if (rps_unresolve_addr(&forward->peer, forward->peername) != RPS_OK) {;
        goto kill;
     }

    //uv_tcp_init must be called before call uv_tcp_connect each time.
    uv_tcp_init(&s->loop, &forward->handle.tcp);

    if (server_connect(forward) != RPS_OK) {
        log_debug("Connect upstream %s:%d failed.", forward->peername, 
               rps_unresolve_port(&forward->peer));
        goto kill;
    }
    
    forward->reconn++;
    return;

kill:
    forward->state = c_kill;
    server_do_next(forward);
}

static void
server_finish(rps_sess_t *sess) {
    /* After retry, still failed*/
    rps_ctx_t   *request;
    rps_ctx_t   *forward;
    // char remoteip[MAX_INET_ADDRSTRLEN];

    request = sess->request;
    forward = sess->forward;

    ASSERT(!forward->established);
    ASSERT(forward->reply_code != rps_rep_ok);

    request->state = c_reply;
    request->reply_code = forward->reply_code;
    server_do_next(request);

    // rps_unresolve_addr(&sess->remote, remoteip);

    // log_info("Establish tunnel %s:%d -> (%s) -> rps -> (%s) -> upstream -> %s:%d failed.",
    //         request->peername, rps_unresolve_port(&request->peer), 
    //         rps_proto_str(request->proto), rps_proto_str(forward->proto), 
    //         remoteip, rps_unresolve_port(&sess->remote));

    forward->state = c_kill;
    server_do_next(forward);
}

static void
server_establish(rps_sess_t *sess) {
    rps_ctx_t   *request;
    rps_ctx_t   *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];


    request = sess->request;
    forward = sess->forward;

    ASSERT(forward->established);
    ASSERT(forward->reply_code == rps_rep_ok);

    request->state = c_reply;
    request->reply_code = forward->reply_code;
    server_do_next(request);
    
    ASSERT(request->rstat == c_stop);
    /* reuqest start read data again */
    server_read_start(request);


    forward->state = c_established;

    rps_unresolve_addr(&sess->remote, remoteip);

    log_info("Establish tunnel %s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d.",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote));
}


static void
server_cycle(rps_ctx_t *ctx) {
    uint8_t    *data;
    size_t     size;
    rps_sess_t  *sess;
    rps_ctx_t   *endpoint;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    sess = ctx->sess;

    endpoint = ctx->flag == c_request? sess->forward:sess->request;

    ASSERT(ctx->state & (c_established | c_pipelined));
    ASSERT(ctx->connected && endpoint->connected);

    if ((ssize_t)size < 0) {

        if ((ssize_t)size == UV_EOF) {
    #ifdef RPS_DEBUG_OPEN
            if (ctx->flag == c_request) {
                log_verb("Client %s finished", sess->request->peername);
            } else {
                log_verb("Upstream %s finished", sess->forward->peername);
            }
    #endif
            server_ctx_close(ctx);
            server_ctx_shutdown(endpoint);
        } else {
            server_ctx_close(ctx);
            server_ctx_close(endpoint);
        } 

        return;
    }

        

    if (server_ctx_dead(endpoint)) {
        return;
    }
    
    if (server_write(endpoint, data, size) != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("redirect %d bytes to %s", size, endpoint->peername);
#endif

}

/* 
 * In tunnel(established) mode, Duplex data forwarding be established.
 * Client <--> RPS <--> Upstream <--> Remote
 */
static void
server_tunnel(rps_ctx_t *ctx) {
    server_cycle(ctx);
}

/*
 * The pipeline mode work in simplex data forwarding
 * Remote ---> Upstream ---> RPS --> Client
 *      forward       request
 */
static void
server_pipeline(rps_ctx_t *ctx) {
    server_cycle(ctx);
}

static void
server_on_forward_close(uv_handle_t* handle) {
    rps_ctx_t *forward;

    forward = handle->data;

    forward->reconn = 0;
    forward->connecting = 0;
    forward->connected = 0;
    forward->established = 0;

    log_debug("Forward to %s be closed.", forward->peername);

    forward->state = c_conn;
    server_do_next(forward);
    return;
}

static void
server_forward_retry(rps_sess_t *sess) {
    struct server *s;
    rps_ctx_t *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];

    s = sess->server;
    forward = sess->forward;

    ASSERT(forward->state == c_retry);
    ASSERT(!forward->established);

    rps_unresolve_addr(&sess->remote, remoteip);

    forward->retry++;

    log_debug("Upstream tunnel  %s -> %s failed, retry: %d", 
            forward->peername, remoteip, forward->retry);


    if (forward->retry >= s->upstreams->maxretry) {
        forward->state = c_failed;
        server_do_next(forward);
        return;
    }


    if (!forward->connected && !forward->connecting) {
        forward->reconn = 0;
        forward->connecting = 0;
        forward->connected = 0;
        forward->established = 0;
        forward->state = c_conn;
        server_do_next(forward);
        return;
    }

    uv_read_stop(&forward->handle.stream);
    uv_timer_stop(&forward->timer);
    uv_close(&forward->handle.handle, server_on_forward_close);
    return;
}

void
server_do_next(rps_ctx_t *ctx) {

    switch (ctx->state) {
        case c_exchange:
            if (ctx->flag == c_request) {
                /* switch context from request to forward and request stop read */
                server_switch(ctx->sess);
            } else {
                /* finish dural context handshake, tunnel established */
                server_establish(ctx->sess);
            }
            break;
        case c_conn:
            server_forward_connect(ctx->sess);
            break;
        case c_retry:
            server_forward_retry(ctx->sess);
            break;
        case c_failed:
            server_finish(ctx->sess);
            break;
        case c_established:
            server_tunnel(ctx);
            break;
        case c_pipelined:
            server_pipeline(ctx);
            break;
        case c_kill:
            server_close(ctx->sess);
            break;
        // case c_closing:
        //     break;
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

    /* wait for upstreams load success */
    uv_mutex_lock(&s->upstreams->mutex);
    uv_cond_wait(&s->upstreams->ready, &s->upstreams->mutex);
    uv_mutex_unlock(&s->upstreams->mutex);

    err = uv_tcp_bind(&s->us, (struct sockaddr *)&s->listen.addr, 0);
    if (err) {
        UV_SHOW_ERROR(err, "bind");
        exit(1);
    }
    
    err = uv_listen((uv_stream_t*)&s->us, TCP_BACKLOG, server_on_request_connect);
    if (err) {
        UV_SHOW_ERROR(err, "listen");
        exit(1);
    }

    log_notice("%s proxy run on %s:%d", s->cfg->proto.data, s->cfg->listen.data, s->cfg->port);

    uv_run(&s->loop, UV_RUN_DEFAULT);
}
