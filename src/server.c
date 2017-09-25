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
    sess->upstream = NULL;
    rps_addr_init(&sess->remote);
    gettimeofday(&sess->start, NULL);
}

static void
server_sess_upstream_mark_fail(rps_sess_t *sess) {
    rps_ctx_t *request, *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];

    if (sess->request == NULL || sess->upstream == NULL) {
        return;
    }

    sess->upstream->failure += 1;

    request = sess->request;
    forward = sess->forward;
    
    rps_unresolve_addr(&sess->remote, remoteip);    

    log_debug("%s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d failed, retry: %d, recon: %d",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote), forward->retry, forward->reconn);
}

static void
server_sess_mark_fail(rps_sess_t *sess) {
    float elapsed;
    rps_ctx_t *request;
    char remoteip[MAX_INET_ADDRSTRLEN];

    request = sess->request;
    /* request may has been free */
    if (request == NULL) {
        return;
    }

    server_sess_upstream_mark_fail(sess);

    gettimeofday (&sess->end, NULL);
    elapsed = (sess->end.tv_sec - sess->start.tv_sec) + 
        ((sess->end.tv_usec - sess->start.tv_usec)/1000000.0);

    if (rps_addr_uninit(&sess->remote)) {
        log_info("%s:%d -> rps:%d failed, used %.2f s'",
                request->peername, rps_unresolve_port(&request->peer), 
                rps_unresolve_port(&sess->server->listen), elapsed);
    } else {
        rps_unresolve_addr(&sess->remote, remoteip);        
        
        log_info("%s:%d -> rps:%d -> upstream -> %s:%d failed, used %.2f s'",
                request->peername, rps_unresolve_port(&request->peer), 
                rps_unresolve_port(&sess->server->listen), 
                remoteip, rps_unresolve_port(&sess->remote), elapsed);
    }
}

static void
server_sess_mark_success(rps_sess_t *sess) {
    float elapsed;
    rps_ctx_t *request, *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];

    request = sess->request;
    forward = sess->forward;

    sess->upstream->success += 1;

    gettimeofday (&sess->end, NULL);
    elapsed = (sess->end.tv_sec - sess->start.tv_sec) + 
        ((sess->end.tv_usec - sess->start.tv_usec)/1000000.0);

    rps_unresolve_addr(&sess->remote, remoteip);    

    log_info("%s:%d -> rps:%d -> %s:%d -> %s:%d success, used %.2f s'",
            request->peername, rps_unresolve_port(&request->peer),
            rps_unresolve_port(&sess->server->listen), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote), elapsed);
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

    sess->upstream = NULL;
    rps_free(sess);
}

static rps_status_t
server_ctx_init(rps_ctx_t *ctx, rps_sess_t *sess, uint8_t flag, uint32_t timeout) {
    ctx->sess = sess;
    ctx->flag = flag;
    ctx->state = c_init;
    ctx->stream = -1;
    ctx->nread = 0;
    ctx->nwrite = 0;
    ctx->nwrite2 = 0;
    ctx->reconn = 0;
    ctx->retry = 0;
    ctx->connecting = 0;
    ctx->connected = 0;
    ctx->established = 0;
    ctx->c_count = 0;
    ctx->proto = UNSET;
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
        rps_free(ctx->wbuf);
        return RPS_ENOMEM;
    }

    ctx->req = NULL;
    ctx->do_next = NULL;

    return RPS_OK;
}

static void 
server_ctx_set_proto(rps_ctx_t *ctx, rps_proto_t proto) {

    ctx->proto = proto;

    if (ctx->proto == SOCKS5) {
        ctx->stream = c_tunnel;
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
        ctx->stream = c_pipeline;
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
        ctx->stream = c_tunnel;
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
    ctx->c_count = 0;

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

    ctx->do_next = NULL;
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

    ctx->c_count += 1;

    if (ctx->c_count < 2) {
        //waitting for both timer and handler closed.
        return; 
    }
    
    switch (ctx->flag) {
        case c_request:
            log_debug("Request from %s:%d be closed", 
                    ctx->peername, rps_unresolve_port(&ctx->peer));
            break;
        case c_forward:
            if (ctx->sess->upstream != NULL) {
                log_debug("Forward to %s:%d be closed.", 
                    ctx->peername, rps_unresolve_port(&ctx->peer));    
            }
            break;
        default:
            NOT_REACHED();
    }

    server_ctx_deinit(ctx);

    server_do_next(ctx);
}



static void
server_ctx_close(rps_ctx_t *ctx) {

    if (server_ctx_dead(ctx)) {
        return;
    }

    uv_timer_stop(&ctx->timer);
    uv_close((uv_handle_t *)&ctx->timer, (uv_close_cb)server_on_ctx_close);

    if (!ctx->connecting && !ctx->connected) {
        // we still need guarantee free the memory of context and session 
        // even if connect didn't established 
        ctx->c_count += 1;
        return;
    }

    ctx->state = c_closing;

    // Recycle the allocated resources.
    // Only context that has been connected need close action
    if (ctx->connected) {
        server_do_next(ctx);    
    }

    uv_read_stop(&ctx->handle.stream);
    uv_close(&ctx->handle.handle, (uv_close_cb)server_on_ctx_close);
}

static void
server_close(rps_sess_t *sess) {
    server_ctx_close(sess->request);
    server_ctx_close(sess->forward);
    server_sess_mark_fail(sess);
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

    if (server_ctx_dead(ctx)) {
        return;
    }
    

    if (ctx->flag == c_request) {
        ctx->state = c_kill;
        log_debug("Request from %s timeout", ctx->peername);
    } else {
        /* tunnel or pipeline has been established retry dosen’t make sense */
        if (ctx->state & c_established) {
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

    if (server_ctx_dead(ctx)) {
        return;
    }

    ASSERT(&ctx->handle.stream == stream);
    if (nread > 0) {
        ASSERT(ctx->rbuf == buf->base);
    }

    ctx->rstat = c_done;
    ctx->nread = nread;

    if (nread <0 ) {
        
        if (ctx->state & c_established) {
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

/*
static void
server_read_stop(rps_ctx_t *ctx) {
    uv_read_stop(&ctx->handle.stream);
    ctx->rstat = c_stop;
}
*/

static void
server_on_write_done(uv_write_t *req, int err) {
    rps_ctx_t *ctx;

    if (err == UV_ECANCELED) {
        return;  /* Handle has been closed. */
    }

    ctx = req->data;

    if (server_ctx_dead(ctx)) {
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

    if (server_ctx_dead(ctx)) {
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

    //ctx->connecting = 0;

    /* request maybe killed before forward connected. */
    if (ctx->flag == c_forward && server_ctx_dead(ctx->sess->request)) {
        ctx->state = c_kill;
    }

    server_do_next(ctx);
}


static rps_status_t
server_connect(rps_ctx_t *ctx) {
    int err;

    ASSERT(!ctx->connected);
    ASSERT(!ctx->connecting);

    //uv_tcp_init must be called before call uv_tcp_connect each time.
    uv_tcp_init(&ctx->sess->server->loop, &ctx->handle.tcp);

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
    
    uv_tcp_init(&s->loop, &request->handle.tcp);
    uv_timer_init(&s->loop, &request->timer);

    err = uv_accept(us, &request->handle.stream);
    if (err) {
        UV_SHOW_ERROR(err, "accept");
        goto error;
    }

    request->connecting = 1;
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

    //ASSERT(sess->forward == NULL);
    // server_switch has been called, 
    // however may double called due to request has invalid new data read.
    if (sess->forward != NULL) {
        return;
    }

    s = sess->server;
    request = sess->request;
    /* request stop read, wait for upstream establishment finished */
    // server_read_stop(request);

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
server_on_forward_close(uv_handle_t* handle) {
    rps_ctx_t *forward;
    rps_ctx_t *request;

    forward = handle->data;
    request = forward->sess->request;

    forward->connecting = 0;
    forward->connected = 0;
    forward->established = 0;

    /* request context may have been free during server_forward_reconn called */
    if (request == NULL) {
        server_ctx_close(forward);
        return;
    }

    server_sess_upstream_mark_fail(forward->sess);

    forward->state = c_conn;
    server_do_next(forward);
    return;
}

static void
server_forward_reconn(rps_ctx_t *forward) {
    struct server *s;

    s = forward->sess->server;

    forward->reconn += 1;
    
    if (forward->reconn > s->upstreams->maxreconn) {
        goto kill;
    }

    if (!forward->connecting) {
        //reconnect directly
        forward->state = c_conn;
        server_do_next(forward);
        return;
    }

    forward->state = c_closing;

    uv_read_stop(&forward->handle.stream);
    uv_timer_stop(&forward->timer);
    uv_close(&forward->handle.handle, server_on_forward_close);
    return;

kill:
    forward->state = c_kill;
    server_do_next(forward);
}

static void
server_forward_connect(rps_ctx_t *forward) {
    struct server *s;
    struct session *sess;

    s = forward->sess->server;
    sess = forward->sess;

    ASSERT(forward->flag == c_forward);
    ASSERT(forward->state = c_conn);


    /* Be called after connect done */
    if (forward->connecting) {

        if (forward->connected) {
            server_ctx_set_proto(forward, sess->upstream->proto);

            /* Connect success */
            log_debug("Connect upstream %s://%s:%d success", rps_proto_str(forward->proto), forward->peername, 
                    rps_unresolve_port(&forward->peer));
            
            if (server_read_start(forward) != RPS_OK) {
                goto reconn;
            }

            /* Set forward protocol after upstream has connected */
            forward->reconn = 0;
            forward->state = c_handshake_req;
            server_do_next(forward);
            return;
        }

        goto reconn;
    }

    sess->upstream = upstreams_get(s->upstreams, sess->request->proto);
    if (sess->upstream == NULL) {
        log_error("no available %s upstream proxy.", rps_proto_str(sess->request->proto));
        forward->state = c_failed;
        forward->reply_code = rps_rep_proxy_unavailable;
        server_do_next(forward);
        return;
    }

    memcpy(&forward->peer, &sess->upstream->server, sizeof(sess->upstream->server));

    if (rps_unresolve_addr(&forward->peer, forward->peername) != RPS_OK) {
        goto reconn;
    }


    if (server_connect(forward) != RPS_OK) {
        log_debug("Connect upstream %s:%d failed. reconn: %d", forward->peername, 
                rps_unresolve_port(&forward->peer), forward->reconn);
        goto reconn;
    }
    
    return;

reconn:
    server_forward_reconn(forward);
    return;
}


static void
server_finish(rps_sess_t *sess) {
    /* After retry, still failed*/
    rps_ctx_t   *request;
    rps_ctx_t   *forward;

    request = sess->request;
    forward = sess->forward;

    ASSERT(!forward->established);
    ASSERT(forward->reply_code != rps_rep_ok);

    request->state = c_reply;
    request->reply_code = forward->reply_code;
    server_do_next(request);

    forward->state = c_kill;
    server_do_next(forward);
}


/* 
 * In tunnel(established) mode, Duplex data forwarding be established.
 *
 *          tunnel             tunnel
 * Remote <--------> Upstream <-------> RPS --> Client
 *         request              forward 
 */
static void
server_establish_tunnel(rps_sess_t *sess) {
    rps_ctx_t   *request;
    rps_ctx_t   *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];

    request = sess->request;
    forward = sess->forward;

    ASSERT(forward->established);
    // ASSERT(forward->reply_code == rps_rep_ok);


    forward->state = c_established;

    request->state = c_reply;
    request->reply_code = forward->reply_code;
    server_do_next(request);
    

    rps_unresolve_addr(&sess->remote, remoteip);

    log_debug("Establish tunnel %s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d.",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote));
}

/*
 * The pipeline mode work in simplex data forwarding
 *
 *        pipeline            pipeline
 * Remote --------> Upstream -------> RPS --> Client
 *        request             forward 
 */
static void
server_establish_pipeline(rps_sess_t *sess) {
    struct context *request, *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];
    

    request = sess->request;
    forward = sess->forward;

    rps_unresolve_addr(&sess->remote, remoteip);

    log_debug("Establish pipeline %s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d.",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote));

    forward->state = c_established;
    request->state = c_established;
    server_do_next(forward);
}

/*
 * The pipeline_tunnel mode kickoff while meet the next three conditions
 * 1. Hybrid enabled 
 * 2. The client request via HTTP proxy
 * 3. Upstream are http_tunnel or socks5 proxy
 *
 *        pipeline             tunnel
 * Remote --------> Upstream <-------> RPS --> Client
 *        request              forward 
 */
static void
server_establish_pipeline_tunnel(rps_sess_t *sess) {
    struct context *request, *forward;
    char remoteip[MAX_INET_ADDRSTRLEN];
    

    request = sess->request;
    forward = sess->forward;

    ASSERT(forward->established);
    // ASSERT(forward->reply_code == rps_rep_ok);

    forward->state = c_established;

    rps_unresolve_addr(&sess->remote, remoteip);

    log_debug("Establish hybrid pipeline_tunnel %s:%d -> (%s) -> rps -> (%s) -> %s:%d -> %s:%d.",
            request->peername, rps_unresolve_port(&request->peer), 
            rps_proto_str(request->proto), rps_proto_str(forward->proto), 
            forward->peername, rps_unresolve_port(&forward->peer), 
            remoteip, rps_unresolve_port(&sess->remote));

    request->state = c_established;
    server_do_next(request);

}

static void
server_establish(rps_sess_t *sess) {
    switch (sess->request->stream) {
    case c_tunnel:
        server_establish_tunnel(sess);
        break;
    case c_pipeline:
        if (sess->forward->stream == c_pipeline) {
            server_establish_pipeline(sess);
        } else if (sess->forward->stream == c_tunnel) {
            server_establish_pipeline_tunnel(sess);
        } else{
            NOT_REACHED();
        }
        break;
    default:
        NOT_REACHED();
    }    
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

    /* Endpoint maybe has been closed and memory has been freed, 
    * however current context still on shutdown state and hasn't been closed right now.
    * Drop the data directly in this approach and watting for current context be closed.
    */
    if (server_ctx_dead(endpoint)) {
        return;
    }

    ASSERT(ctx->state & c_established);
    ASSERT(ctx->connected && endpoint->connected);

    if ((ssize_t)size < 0) {

        if ((ssize_t)size == UV_EOF) {
            server_ctx_close(ctx);
            server_ctx_shutdown(endpoint);
            server_sess_mark_success(sess);
        } else {
            ctx->state = c_kill;
            server_do_next(ctx);
        } 
        return;
    }
    
    if (server_write(endpoint, data, size) != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("redirect %d bytes to %s:%d", 
            size, endpoint->peername, rps_unresolve_port(&endpoint->peer));
#endif

}

static void
server_forward_retry(rps_ctx_t *forward) {
    struct server *s;
    char remoteip[MAX_INET_ADDRSTRLEN];

    s = forward->sess->server;

    ASSERT(forward->flag == c_forward);
    ASSERT(forward->state == c_retry);
    ASSERT(!forward->established);

    rps_unresolve_addr(&forward->sess->remote, remoteip);

    forward->retry++;

    if (forward->retry > s->upstreams->maxretry) {
        forward->state = c_failed;
        server_do_next(forward);
        return;
    }


    forward->reconn = 0;
    server_forward_reconn(forward);
    return;
}

void
server_do_next(rps_ctx_t *ctx) {

    switch (ctx->state) {
        case c_exchange:
            server_switch(ctx->sess);
            break;
        case c_conn:
            server_forward_connect(ctx);
            break;
        case c_retry:
            server_forward_retry(ctx);
            break;
        case c_failed:
            server_finish(ctx->sess);
            break;
        case c_establish:
            server_establish(ctx->sess);
            break;
        case c_established:
            server_cycle(ctx);
            break;
        case c_will_kill:
            server_ctx_shutdown(ctx);
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
            // ctx->do_next may be null while server_ctx_set_proto hasn't been called.
            if (ctx->do_next != NULL) {
                ctx->do_next(ctx);    
            }
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
