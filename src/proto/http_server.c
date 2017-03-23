#include "http.h"
#include "core.h"

#include <uv.h>

static size_t
http_read_line(uint8_t *data, size_t start, size_t end, rps_str_t *str) {
    size_t i, n, len;
    uint8_t c, last;

    ASSERT(string_empty(str));

    n = 0;
    len = 1;

    for (i=start; i<end; i++, len++) {
        c = data[i];
        if (c == LF) {
            if (last == CR) {
                n = len - CRLF_LEN;
            } else {
                n = len - LF_LEN;
            }

            if (n > 0) {
                string_duplicate(str, &data[start], n);
            }
            break;
        }

        last = c;
    }

    return len;
}

static rps_status_t
http_parse_request(rps_str_t *str, struct http_request *req) {
    int ret;
    char method[16];
    char url[128];
    char protocol[16];

    ret = sscanf(str->data, "%[^ ] %[^ ] %[^ ]", method, url, protocol);
    
    printf("method:%s\n", method);
    printf("url:%s\n", url);
    printf("protocol:%s\n", protocol);

    
    return RPS_OK;
}




static void
http_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    int i, len;
    int line;
    int cr;
    rps_str_t *str;
    struct http_request req;
    
    i = 0;
    line = 0;

    for (;;) {
        str =  string_new();
        if (str == NULL) {
            goto kill;
        }
        len = http_read_line(data, i, size, str);
        if (len <= CRLF_LEN) {
            /* read empty line, just contain /r/n */
            break;
        }

        i += len;
        line++;

        if (line == 1) {
            http_parse_request(str, &req);
            
        }

        printf("line <%d>: %s\n", line, str->data);
        string_free(str);
    }

    if ((size != i + 2 * CRLF_LEN) && (size != i + CRLF_LEN)) {
        log_error("http tunnel handshake contain junk: %s", data);
        /* 2*CRLF_LEN == last line \r\n\r\n */
        goto kill;
    }
    
kill:
    ctx->state = c_kill;
    server_do_next(ctx);

}

static void
http_do_auth(struct context *ctx, uint8_t *data, size_t size) {
    printf("begin do http auth, read %zd bytes: %s\n", size, data);

}


void
http_server_do_next(struct context *ctx) {
    uint8_t *data;
    ssize_t size;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;


    switch (ctx->state) {
        case c_handshake:
            http_do_handshake(ctx, data, size);
            break;
        case c_auth:
            http_do_auth(ctx, data, size);
            break;
        default:
            NOT_REACHED();
    }
}

