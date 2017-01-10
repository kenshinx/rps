
#include "s5.h"
#include "server.h"

#include <stdio.h>
#include <uv.h>



void 
s5_do_next(struct context *ctx, const char *data, ssize_t nread) {
    printf("<<nread:%zd>>, %s\n", nread, data);
}

uint16_t
s5_server_handshake(struct context *ctx) {
    
}

uint16_t
s5_server_auth(struct context *ctx) {

}

static s5_phase_t 
s5_request() {

}

static s5_phase_t
s5_reply() {

}


static s5_err_t
s5_parse() {
}

