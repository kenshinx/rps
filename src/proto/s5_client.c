#include "s5.h"
#include "core.h"
#include "upstream.h"

#include <stdio.h>




static void
s5_do_handshake(struct context *ctx) {
    rps_status_t status;
    struct session  *sess;
    struct s5_method_request req;

    sess = ctx->sess;

    req.ver = SOCKS5_VERSION;
    if (string_empty(&sess->upstream.uname)) {
        req.nmethods = 1;
        req.methods[0] = 0x00;
        status = server_write(ctx, &req, 3);
    } else {
        req.nmethods = 2;
        req.methods[0] = 0x00;
        req.methods[1] = 0x02;
        status = server_write(ctx, &req, 4);
    }

    if (status != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
        return;
    }
    
    ctx->state = c_handshake_resp;
    return;
}

static void
s5_do_handshake_resp(struct context *ctx) {
    uint8_t    *data;
    size_t     size;
    ctx_state_t new_state;
    struct s5_method_response *resp;
    
    data = (uint8_t *)ctx->buf;
    size = (size_t)ctx->nread;

    if (size != 2) {
        log_warn("junk in handshake");
        goto kill;
    }

    resp = (struct s5_method_response *)data;
    if (resp->ver != SOCKS5_VERSION) {
        log_warn("s5 handshake error: bad protocol version.");
        goto kill;
    }

    switch (resp->method) {
        case s5_auth_none:
            new_state = c_requests;
            break;
        case s5_auth_passwd:
            new_state = c_auth;
            break;
        case s5_auth_gssapi:
        case s5_auth_unacceptable:
        default:
            new_state = c_kill;
            log_warn("s5 handshake error: unacceptable authentication.");
            break;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("s5 client handshake finish.");
#endif

    ctx->state = new_state;
    server_do_next(ctx);
    return;

kill:
    ctx->state = c_kill;
    server_do_next(ctx);
}

static void
s5_do_auth(struct context *ctx) {
    //struct s5_auth_request req;
    uint8_t req[512];
    struct upstream *u;
    int len;

    len = 0;
    u = &ctx->sess->upstream;

    req[len++] = SOCKS5_AUTH_PASSWD_VERSION;
    req[len++] = u->uname.len;

    if (!string_empty(&u->uname)) {
        memcpy(&req[len], u->uname.data, u->uname.len);
        len += u->uname.len;
    }

    req[len++] = u->passwd.len;

    if (!string_empty(&u->passwd)) {
        memcpy(&req[len], u->passwd.data, u->passwd.len);
        len += u->passwd.len;
    } 

    if (server_write(ctx, req, len) != RPS_OK) {
        ctx->state = c_kill;
    } else {
        ctx->state = c_auth_resp;
    }
}

static void
s5_do_auth_resp(struct context *ctx) {
    uint8_t    *data;
    size_t     size;
    struct s5_auth_response *resp;

    data = (uint8_t *)ctx->buf;
    size = (size_t)ctx->nread;

    if (size != 2) {
        log_warn("junk in auth response");
        goto kill;
    }

    resp = (struct s5_auth_response *)data;

    if (resp->ver != SOCKS5_AUTH_PASSWD_VERSION){
        log_warn("auth version is invalid: %d", resp->ver);
        goto kill;
    }

    if (resp->status != s5_auth_allow) {
        log_warn("auth denied");
        goto kill;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("s5 client auth allow.");
#endif

    ctx->state = c_requests;
    server_do_next(ctx);
    return;

kill:
    ctx->state = c_kill;
    server_do_next(ctx);
}


static void
s5_do_request(struct context *ctx) {
    //struct s5_request *req;
    uint8_t req[512];
    struct session  *sess;
    rps_addr_t *remote;
    int len, alen;

    len = 0;
    sess = ctx->sess;
    remote = &sess->remote;
    
    req[len++] = SOCKS5_VERSION;
    req[len++] = s5_cmd_tcp_connect; //cmd
    req[len++] = 0x00; //rsv
    
    switch (remote->family) {
        case AF_INET:
            req[len++] = s5_atyp_ipv4;
            memcpy(&req[len], &remote->addr.in.sin_addr, 4);
            len += 4;
            memcpy(&req[len], &remote->addr.in.sin_port, 2);
            break;
        case AF_INET6:
            req[len++] = s5_atyp_ipv6;
            memcpy(&req[len], &remote->addr.in.sin_addr, 16);
            len += 16;
            memcpy(&req[len], &remote->addr.in6.sin6_port, 2);
            break;
        case AF_DOMAIN:
            req[len++] = s5_atyp_domain;
            alen = strlen(remote->addr.name.host);
            req[len++] = alen;
            memcpy(&req[len], (const char *)remote->addr.name.host, alen);
            len += alen;
            memcpy(&req[len], &remote->addr.name.port, 2);
            break;
        default:
            NOT_REACHED();
    }

    len += 2; //port length = 2

    if (server_write(ctx, req, len) != RPS_OK) {
        ctx->state = c_kill;
    } else {
        ctx->state = c_reply;
    }
}


static void
s5_do_reply(struct context *ctx) {
    uint8_t    *data;
    size_t     size;
    struct s5_in4_response *resp;

    data = (uint8_t *)ctx->buf;
    size = (size_t)ctx->nread;

    resp = (struct s5_in4_response *)data; 
    if (resp->ver != SOCKS5_VERSION) {
        log_warn("s5 reply error: bad protocol version.");
        goto kill;
    }

    if (resp->rep != s5_rep_success) {
        log_warn("s5 reply failed by error id: '%d'", resp->rep);
    } else {
    #ifdef RPS_DEBUG_OPEN
        log_verb("s5 reply success.");
    #endif
    }

    ctx->state = c_exchange;
    server_do_next(ctx);
    return;
    
kill:
    ctx->state = c_kill;
    server_do_next(ctx);
}

void 
s5_client_do_next(struct context *ctx) {

    switch(ctx->state) {
        case c_handshake:
            s5_do_handshake(ctx);
            break;
        case c_handshake_resp:
            s5_do_handshake_resp(ctx);
            break;
        case c_auth:
            s5_do_auth(ctx);
            break;
        case c_auth_resp:
            s5_do_auth_resp(ctx);
            break;
        case c_requests:
            s5_do_request(ctx);
            break;
        case c_reply:
            s5_do_reply(ctx);
            break;
        default:
            NOT_REACHED();
    }
    
}
