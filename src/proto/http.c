#include "http.h"
#include "server.h"

#include <uv.h>


void 
http_do_next(struct context *ctx, const char *data, ssize_t nread) {
    printf("<<nread:%zd>>, %s\n", nread, data);
}

uint16_t
http_server_handshake(struct context *ctx) {
    
}

uint16_t
http_server_auth(struct context *ctx) {

}

uint16_t
http_client_handshake(struct context *ctx) {
    
}

uint16_t
http_client_auth(struct context *ctx) {

}

