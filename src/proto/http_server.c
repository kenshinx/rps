#include "http.h"
#include "core.h"
#include "util.h"

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
    uint8_t *start, *end;
    uint8_t ch;
    size_t i, len;

    enum {
        sw_start = 0,
        sw_method,
        sw_space_before_host,
        sw_host,
        sw_port,
        sw_space_before_protocol,
        sw_protocol,
        sw_end,
    } state;

    state = sw_start;

    for (i = 0; i < str->len; i++) {

        ch = str->data[i];

        switch (state) {
            case sw_start:
                start = &str->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_method;
                break;

            case sw_method:
                if (ch == ' ') {
                    /* method end */
                    end = &str->data[i];
                    
                    switch (end - start) {
                        case 3:
                            if (rps_str4_cmp(start, 'G', 'E', 'T', ' ')) {
                                req->method = http_get;
                                break;
                            }
                            break;
                        case 4:
                            if (rps_str4_cmp(start, 'P', 'O', 'S', 'T')) {
                                req->method = http_post;
                                break;
                            }
                            break;
                        case 7:
                            if (rps_str7_cmp(start, 'C', 'O', 'N', 'N', 'E', 'C', 'T')) {
                                req->method = http_connect;
                                break;
                            }
                            break;
                        default:
                            break;
                    }

                    start = end;
                    state = sw_space_before_host;
                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_') {
                    log_error("http parse request line error, invalid method");
                    return RPS_ERROR;
                }
                break;

            case sw_space_before_host:
                start = &str->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_host;
                break;

            case sw_host:
                if (ch == ':') {
                    end = &str->data[i];
                    if (end - start <= 0) {
                        log_error("http parse request line error, invalid host");
                        return RPS_ERROR;
                    }
                    string_duplicate(&req->host, (const char *)start, end - start);
                    start = &str->data[i+1]; /* cross ':' */
                    state = sw_port;
                    break;
                }

                if (ch == ' ') {
                    log_error("http parse request line error, need port");
                    return RPS_ERROR;
                }

                 
                /* rule is not too strict, adapt to punycode encode doamin */
                if (ch < '-' || ch > 'z') {
                    log_error("http parse request line error, invalid host");
                    return RPS_ERROR;
                }
                break;

            case sw_port:
                if (ch >= '0' && ch <= '9') {
                    break;
                }

                if (ch == ' ') {
                    end = &str->data[i];
                    len = end - start;

                    if (len <=0 || len >= 6) {
                        log_error("http parse request line error, invalid port");
                        return RPS_ERROR;
                    }

                    for (; start < end; start++) {
                        req->port = req->port * 10 + (*start - '0'); 
                    }

                    start = end;
                    state = sw_space_before_protocol;
                    break;
                }
                
                log_error("http parse request line error, invalid port");
                return RPS_ERROR;

            case sw_space_before_protocol:
                start = &str->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_protocol;
                break;

            case sw_protocol:
                if (ch == ' ') {
                    state = sw_end;
                    break;
                }

                end = &str->data[i];
                break;

            case sw_end:
                if (ch != ' ') {
                    log_error("http parse request line error, junk in request line");
                    return RPS_ERROR;
                }
            
            default:
                NOT_REACHED();
        }
    }

    if (end - start <= 0) {
        log_error("http parse request line error, invalid protocol");
        return RPS_ERROR;
    }

    string_duplicate(&req->protocol, (const char *)start, end - start +1);

    if (state != sw_protocol && state != sw_end) {
        log_error("http parse request line error, parse failed");
        return RPS_ERROR;
    }
    
    return RPS_OK;
}

static rps_status_t
http_check_request(struct http_request *req) {
    if (req->method != http_connect) {
        log_error("http request check error, only connect support");
        return RPS_ERROR;
    }

    if (!rps_valid_port(req->port)) {
        log_error("http request check error, invalid port");
        return RPS_ERROR;
    }

    return RPS_OK;
} 

#ifdef RPS_DEBUG_OPEN
static void
http_request_dump(struct http_request *req) {

    char *method;

    switch (req->method) {
        case http_get:
            method = "GET";
            break;
        case http_post:
            method = "POST";
            break;
        case http_connect:
            method = "CONNECT";
            break;
        default:
            method = "UNKNOWN";
    }

    log_verb("[http request]");
    log_verb("%s %s:%d %s", method, req->host.data, 
            req->port, req->protocol.data);
}
#endif


static void
http_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    size_t i, len;
    int line;
    rps_str_t str;
    struct http_request req;

    http_request_init(&req);
    
    i = 0;
    line = 0;

    for (;;) {
        string_init(&str);

        len = http_read_line(data, i, size, &str);
        if (len <= CRLF_LEN) {
            /* read empty line, only contain /r/n */
            break;
        }


        i += len;
        line++;

        if (line == 1) {
            if (http_parse_request_line(&str, &req) != RPS_OK) {
                log_error("parse http request line: %s error.", str.data);
                goto kill;
            }
            
            if (http_check_request(&req) != RPS_OK) {
                log_error("invalid http request: %s", str.data);
                goto kill;
            }
        }

        printf("line <%d>: %s\n", line, str.data);
        string_deinit(&str);
    }

    if ((size != i + 2 * CRLF_LEN) && (size != i + CRLF_LEN)) {
        log_error("http tunnel handshake contain junk: %s", data);
        /* 2*CRLF_LEN == last line \r\n\r\n */
        goto kill;
    }

#ifdef RPS_DEBUG_OPEN
    http_request_dump(&req);
#endif
    
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

