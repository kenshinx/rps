
#include "s5.h"
#include "core.h"
#include "util.h"
#include "log.h"
#include "server.h"

#include <stdio.h>
#include <uv.h>

static uint8_t
s5_select_auth(struct s5_method_request *req) {
    int i;
    uint8_t method;

    ASSERT(req->nmethods <= 255);

    /* Select no authentication required if rps server didn't set user and password */

    for (i=0; i<req->nmethods; i++) {
        method = req->methods[i];
        if (method == s5_auth_none) {
            return s5_auth_none;
        }
    }

    return s5_auth_unacceptable;
}

static void
s5_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    ctx_state_t new_state;
    struct server *s;
    struct s5_method_request *req;
    struct s5_method_response resp;
    rps_status_t status;

    req = (struct s5_method_request *)data;
    if (req->ver != SOCKS5_VERSION) {
        log_error("s5 handshake error: bad protocol version.");
        goto kill;
    }

    /* ver(1) + nmethods(1) + methods(nmethods) */
    if (size != (2 + req->nmethods)) {
        log_error("junk in handshake");
        goto kill;
    }

    memset(&resp, 0, sizeof(struct s5_method_response));

    resp.ver = req->ver;

    s = ctx->sess->server;
    
    if (string_empty(&s->cfg->username) && string_empty(&s->cfg->password)) {
        /* If rps didn't assign username and password, 
         * select auth method dependent on client request 
         * */
        resp.method = s5_select_auth(req);
    } else {
        resp.method = s5_auth_passwd;
    }

    status = server_write(ctx, &resp, sizeof(resp));
    if (status != RPS_OK) {
        goto kill;
    }

    switch (resp.method) {
        case s5_auth_none:
            new_state = c_requests;
            break;
        case s5_auth_passwd:
            new_state = c_auth;
            break;
        case s5_auth_gssapi:
        case s5_auth_unacceptable:
            new_state = c_kill;
            log_error("s5 handshake error: unacceptable authentication.");
            break;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("s5 handshake finish.");
#endif
    
    ctx->state = new_state;
    return;

kill:
    ctx->state=c_kill;
    server_do_next(ctx);
    return;
}

static void
s5_do_auth(struct context *ctx, uint8_t *data, size_t size) {
    ctx_state_t new_state;
    struct server *s;
    struct s5_auth_request *req;
    struct s5_auth_response resp;
    rps_status_t status;

    req = (struct s5_auth_request *)data;
    if (req->ver != SOCKS5_AUTH_PASSWD_VERSION) {
        log_error("s5 handshake error: bad password auth version.");
        goto kill;
    } 

    /* Reset the req struct memory layout */
    req->plen = req->uname[req->ulen];
    req->uname[req->ulen] = '\0';
    memcpy(req->passwd, &req->uname[req->ulen+1], req->plen);
    req->passwd[req->plen] = '\0';

    /* ver(1) + ulen(1) + uname(ulen) + plen(1) + passwd(plen) */
    if ((3 + req->ulen + req->plen) != size) {
        log_error("junk in auth");
        goto kill;
    }

    if ((strlen((const char *)req->uname) != req->ulen) || 
        (strlen((const char *)req->passwd) != req->plen)) {
        log_error("invalid auth packet");
        goto kill;
        
    }

    s = ctx->sess->server;

    memset(&resp, 0, sizeof(struct s5_auth_response));

    resp.ver = SOCKS5_AUTH_PASSWD_VERSION;
    if (rps_strcmp(&s->cfg->username, req->uname) == 0 && 
        rps_strcmp(&s->cfg->password, req->passwd) == 0) {
        resp.status = s5_auth_allow;
        new_state = c_requests;
    } else {
        resp.status = s5_auth_deny;
        new_state = c_kill;
    }

    status = server_write(ctx, &resp, sizeof(resp));
    if (status != RPS_OK) {
        goto kill;
    }

#ifdef RPS_DEBUG_OPEN
    if (resp.status ==  s5_auth_allow) {
        log_verb("s5 username password authentication success.");
    } else {
        log_verb("s5 username password authentication failed.");
    }
#endif

    ctx->state = new_state;
    return;

kill:
    ctx->state=c_kill;
    server_do_next(ctx);
    return;
}

static void
s5_do_request(struct context *ctx, uint8_t *data, size_t size) {
    uint8_t alen;
    struct s5_request *req;
    struct s5_in4_response resp;
    char remoteip[MAX_INET_ADDRSTRLEN];
    int err;
    rps_addr_t  *remote;

    req = (struct s5_request *)data;
    if (req->ver != SOCKS5_VERSION) {
        log_error("s5 request error: bad protocol version.");
        goto kill;
    }

    s5_in4_response_init(&resp);

    if (req->cmd != s5_cmd_tcp_connect) {
        /* Command not supported */
        resp.rep = 0x07;
        server_write(ctx, &resp, sizeof(struct s5_in4_response));
        log_error("s5 request error: only support tcp connect verify.");
        goto kill;
    }

    remote = &ctx->sess->remote;
    
    switch (req->atyp) {
        case s5_atyp_ipv4:
            alen = 4;
            memcpy(req->dport, &req->daddr[alen], 2);
            rps_addr_in4(remote, req->daddr, alen, req->dport);
            break;

        case s5_atyp_ipv6:
            alen = 16;
            memcpy(req->dport, &req->daddr[alen], 2);
            rps_addr_in6(remote, req->daddr, alen, req->dport);
            break;

        case s5_atyp_domain:
            /* First byte is hostname length */
            alen = req->daddr[0]; 
            /* Last 2 byte is dport */
            memcpy(req->dport, &req->daddr[alen+1], 2);  
            rps_addr_name(remote, &req->daddr[1], alen, req->dport);
            break;

        default:
            /* Address type not supported */
            resp.rep = 0x08;
            server_write(ctx, &resp, sizeof(struct s5_in4_response));
            goto kill;
    }

    /*
     * ver(1) + cmd(1) + rsv(1) + atyp(1) +daddr(alen) + dport(2) 
     * In some case of atyp_domain, alen didn't calculate the last '\0' 
     * suffix with hostname, in order fix this pattern, so we need size -1.
     */
    if ((6 + alen) != size && (6 + alen) != (size-1)) {
        log_error("junk in request");
        goto kill;
    }

    err = rps_unresolve_addr(remote, remoteip);
    if (err < 0) {
        goto kill;
    }
    log_debug("remote %s:%d", remoteip, rps_unresolve_port(remote));

    ctx->state = c_reply_pre;
    server_do_next(ctx);
    return;

kill:
    ctx->state=c_kill;
    server_do_next(ctx);
    return;
}

static void
s5_do_reply() {
    return;
}

void 
s5_server_do_next(struct context *ctx) {
    uint8_t    *data;
    size_t     size;

    data = (uint8_t *)ctx->buf;
    size = (size_t)ctx->nread;

    switch (ctx->state) {
        case c_handshake:
            s5_do_handshake(ctx, data, size);
            break;
        case c_auth:
            s5_do_auth(ctx, data, size);
            break;
        case c_requests:
            s5_do_request(ctx, data, size);
            break;
        case c_reply:
            s5_do_reply();
            break;
        default:
            NOT_REACHED();
    }

}
