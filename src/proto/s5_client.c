#include "s5.h"

#include <stdio.h>


void 
s5_do_next(struct context *ctx, const char *data, ssize_t nread) {
    printf("<<nread:%zd>>, %s\n", nread, data);
}


uint16_t
s5_client_handshake(struct context *ctx) {
    
}

uint16_t
s5_client_auth(struct context *ctx) {

}


