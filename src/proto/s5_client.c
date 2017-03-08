#include "s5.h"
#include "core.h"

#include <stdio.h>



ctx_state_t 
s5_client_do_next(struct context *ctx) {
    printf("%s switch to forward conext", ctx->peername);
}

ctx_state_t
s5_client_handshake() {
    
}

ctx_state_t
s5_client_auth() {

}


