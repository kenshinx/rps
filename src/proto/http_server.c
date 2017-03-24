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
                string_duplicate(str, (const char *)&data[start], n);
            }
            break;
        }

        last = c;
    }

    return len;
}

static rps_status_t
http_parse_request_line(rps_str_t *str, struct http_request *req) {
    uint8_t *p, *end;
    uint8_t ch;
    size_t i;

    enum {
        sw_start = 0,
        sw_method,
        sw_space_before_host,
        sw_host,
        sw_port,
    } state;

    state = sw_start;

    for (i = 0; i < str->len; i++) {

        ch = str->data[i];

        switch (state) {
            case sw_start:
                p = &str->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_method;
                break;

            case sw_method:
                if (ch == ' ') {
                    /* method end */
                    end = &str->data[i];
                    
                    switch (end - p) {
                        case 3:
                            if (rps_str4_cmp(p, 'G', 'E', 'T', ' ')) {
                                req->method = http_get;
                                break;
                            }
                            break;
                        case 4:
                            if (rps_str4_cmp(p, 'P', 'O', 'S', 'T')) {
                                req->method = http_post;
                                break;
                            }
                            break;
                        case 7:
                            if (rps_str7_cmp(p, 'C', 'O', 'N', 'N', 'E', 'C', 'T')) {
                                req->method = http_connect;
                                break;
                            }
                            break;
                        default:
                            break;
                    }

                    p = &str->data[i];
                    state = sw_space_before_host;
                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_') {
                    log_error("http parse request line error, invalid method");
                    return RPS_ERROR;
                }
                break;

            case sw_space_before_host:
                p = &str->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_host;
                break;

            case sw_host:
                if (ch == ':') {
                    end = &str->data[i];
                    if (end - p <= 0) {
                        log_error("http parse request line error, invalid host");
                        return RPS_ERROR;
                    }
                    string_duplicate(&req->host, (const char *)p, end - p);
                    p = &str->data[i];
                    state = sw_port;
                    break;
                }

                if (ch == ' ') {
                    log_error("http parse request line error, need port");
                    return RPS_ERROR;
                }

                 
                /* rule is not strict, adapt to punycode encode doamin */
                if (ch < '-' || ch > 'z') {
                    log_error("http parse request line error, invalid host");
                    return RPS_ERROR;
                }
                break;

            
            default:
                return RPS_OK;
        }
        
    }

    
    


    
    return RPS_OK;
}


static void
http_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    size_t i, len;
    int line;
    rps_str_t *str;
    struct http_request req;

    http_request_init(&req);
    
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
            http_parse_request_line(str, &req);
            printf("request method: %d\n", req.method);
            printf("request host: %s\n", req.host.data);
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

