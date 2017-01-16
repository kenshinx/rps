#include "server.h"
#include "core.h"
#include "_string.h"
#include "util.h"
#include "proto/s5.h"
#include "proto/http.h"


rps_status_t
server_init(struct server *s, struct config_server *cfg) {
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
    /*
    if (sess->request != NULL) {
        ASSERT(!(sess->request->state &  c_connect));
    }
    if (sess->forward != NULL) {
        ASSERT(!(sess->forward->state &  c_connect));
    }
	*/
    rps_free(sess);
}

static void
server_ctx_init(rps_ctx_t *ctx, rps_sess_t *sess, uint8_t flag, rps_proxy_t proxy) {
    ctx->sess = sess;
    ctx->flag = flag;
    ctx->state = c_init;
    ctx->proxy = proxy;
	ctx->nread = 0;
    ctx->last_status = 0;
    ctx->handle.handle.data  = ctx;
    ctx->write_req.data = ctx;
    ctx->timer.data = ctx;

	if (ctx->proxy == SOCKS5) {
		s5_handle_init(&ctx->proxy_handle.s5);
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
	} else if (ctx->proxy == HTTP) {
		http_handle_init(&ctx->proxy_handle.http);
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
	} else {
		NOT_REACHED();
	}
	
/*
	if (ctx->flag == c_request) {
		switch (ctx->proxy) {
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
		switch (ctx->proxy) {
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

    switch (ctx->flag) {
        case c_request:
            log_debug("Request from %s be closed", ctx->peername);
            sess->request = NULL;       
            break;
        case c_forward:
            log_debug("Forward to %s be closed.", ctx->peername);
            sess->forward = NULL;       
            break;
        default:
            NOT_REACHED();
    }

    rps_free(ctx);
}

static void
server_ctx_close(rps_ctx_t *ctx) {
    //ASSERT((ctx->state & c_connect));
    ctx->state = c_closing;
    uv_close(&ctx->handle.handle, (uv_close_cb)server_on_ctx_close);
    uv_timer_stop(&ctx->timer);
}


static uv_buf_t *
server_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    rps_ctx_t *ctx;

    ctx = handle->data;

	buf->base = ctx->buf;
    buf->len = sizeof(ctx->buf);

    return buf;
}

static void
server_do_next(rps_ctx_t *ctx) {

    if (ctx->last_status < 0) {
        ctx->state = c_kill;
    }

	switch (ctx->state) {
		case c_established:
			/* rps connect has be established */
			break;
        case c_kill:
            break;
        case c_dead:
            break;
		default:
			ctx->do_next(ctx);
	}

	/*
	switch (ctx->state) {
		case c_handshake:
			new_state = ctx->do_handshake(ctx->proxy_handle.s5_handle);	
			break;
		case c_auth:
			new_state = ctx->do_auth(ctx->proxy_handle.s5_handle);	
			break;
		case c_established:
			break;
		default:
			NOT_REACHED();
	}

	ctx->state = new_state;
	*/
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
        server_ctx_close(ctx);
        server_sess_free(ctx->sess);
        return;

    }
	
	ctx->nread = nread;

	server_do_next(ctx);
}

static void
server_on_write_done(uv_write_t *req, int err) {
    rps_ctx_t *ctx;

    ctx = req->data;
    
    if (err) {
        UV_SHOW_ERROR(err, "on write done");
        ctx->last_status = err;
        server_do_next(ctx);
    }
    
    //server_do_next(ctx);
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
    int i;
    log_verb("write %zd bytes", len);
    for (i=0; i<len; i++) {
        log_verb("\t%x", buf.base[i]);
    }
#endif
    
    return RPS_OK;
}


static void 
server_on_timer_expire(uv_timer_t *handle) {
    rps_ctx_t *ctx;

    ctx = handle->data;

	if (ctx->flag == c_request) {
		log_debug("Request from %s timeout", ctx->peername);
	} else {
		log_debug("Forward to %s timeout", ctx->peername);
	}

    server_ctx_close(ctx);
    server_sess_free(ctx->sess);
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
    server_ctx_init(request, sess, c_request, s->proxy);
    
    uv_tcp_init(us->loop, &request->handle.tcp);
    uv_timer_init(us->loop, &request->timer);

    err = uv_accept(us, &request->handle.stream);
    if (err) {
        UV_SHOW_ERROR(err, "accept");
        goto error;
    }

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
    
    err = rps_unresolve_addr(&sess->client, request->peername);
    if (err < 0) {
        log_error("unresolve peername failer.");
        goto error;
    }

    log_debug("Accept request from %s", request->peername);

	request->state = c_handshake;

    /*
     * Beigin receive data
     */
    err = uv_read_start(&request->handle.stream, 
            (uv_alloc_cb)server_alloc, (uv_read_cb)server_on_read_done);
    if (err < 0) {
        goto error;
    }

    /*
     * Set request context timer
     */
    err = uv_timer_start(&request->timer, 
            (uv_timer_cb)server_on_timer_expire, REQUEST_CONTEXT_TIMEOUT, 0);
    if (err) {
        UV_SHOW_ERROR(err, "set request timer");
        goto error;
    }

    return;

error:
    server_ctx_close(request);
    server_sess_free(sess);
    return;
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
