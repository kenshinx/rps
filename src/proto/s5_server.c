
#include "s5.h"
#include "core.h"
#include "util.h"
#include "log.h"

#include <stdio.h>
#include <uv.h>

#define SOCKS5_VERSION  5


static s5_err_t 
s5_parse(s5_handle_t *handle, uint8_t **data, ssize_t *size) {
    s5_err_t err;
    uint8_t *p;
    uint8_t c;
    size_t i;
    size_t n;

    i = 0;
    n = *size; 
    p = *data;

    while(i < n) {
        c = p[i];
        i++;

        printf("read: %x\n", c);
        
        switch (handle->state) {
            case s5_version:
                if (c != SOCKS5_VERSION) {
                    err = s5_bad_version;
                    goto out;
         
                }
                handle->version = c;
                handle->state = s5_nmethods;
                break;

            case s5_nmethods:
                handle->nmethods = c;
                handle->__n = 0;
                handle->state = s5_methods;
                break;

            case s5_methods:
                if (handle->__n < handle->nmethods) {
                    switch (c) {
                        case 0:
                            handle->methods |= s5_auth_none;
                            break;
                        case 1:
                            handle->methods |= s5_auth_gssapi;
                            break;
                        case 2:
                            handle->methods |= s5_auth_passwd;
                            break;
                    }
                    handle->__n++;
                }
                if (handle->__n == handle->nmethods) {
                    printf("nmethods:%d, methods:%c\n", handle->nmethods, handle->methods);
                }
        }
    }

    err = s5_ok;

out:
    *data = p + i;
    *size = n - i;  
    return err;  
    
}

static uint16_t
s5_do_handshake(s5_handle_t *handle, uint8_t *data, ssize_t size) {
    s5_err_t err;
    
    err = s5_parse(handle, &data, &size);
    if (err != s5_ok) {
        log_error("s5 handshake error: %s", s5_strerr(err));
    }

}

static uint16_t
s5_do_auth(s5_handle_t *handle, uint8_t *data, ssize_t size) {

}

static s5_state_t 
s5_request() {

}

static s5_state_t
s5_reply() {

}

void 
s5_server_do_next(struct context *ctx) {
    uint8_t    *data;
    ssize_t size;
    uint16_t new_state; 
    s5_handle_t *handle;

    handle = &ctx->proxy_handle.s5;

    data = (uint8_t *)ctx->buf;
    size = ctx->nread;

    switch (ctx->state) {
        case c_handshake:
            new_state = s5_do_handshake(handle, data, size);
            break;
        case c_auth:
            new_state = s5_do_auth(handle, data, size);
            break;
        default:
            NOT_REACHED();
    }
    
    ctx->state = new_state;
}
