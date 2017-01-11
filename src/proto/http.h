#ifndef _RPS_HTTP_H
#define _RPS_HTTP_H

#include <uv.h>

/*
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 */

typedef struct {
    void    *data;
    uint8_t t;
    
} http_handle_t;

typedef void (* http_next_t)(http_handle_t *);

void http_server_do_next(http_handle_t *handle);
void http_client_do_next(http_handle_t *handle);

uint16_t http_server_handshake(http_handle_t *handle);
uint16_t http_server_auth(http_handle_t *handle);
uint16_t http_client_handshake(http_handle_t *handle);
uint16_t http_client_auth(http_handle_t *handle);


#endif
