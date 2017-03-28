#include "http.h"
#include "core.h"
#include "util.h"

#include <uv.h>

#define HTTP_HEADER_MAX_KEY_LENGTH     256
#define HTTP_HEADER_MAX_VALUE_LENGTH   512

static size_t
http_read_line(uint8_t *data, size_t start, size_t end, rps_str_t *line) {
    size_t i, n, len;
    uint8_t c, last;

    ASSERT(string_empty(line));

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
                string_duplicate(line, (const char *)&data[start], n);
            }
            break;
        }

        last = c;
    }

    return len;
}

static rps_status_t
http_parse_request_line(rps_str_t *line, struct http_request *req) {
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

    for (i = 0; i < line->len; i++) {

        ch = line->data[i];

        switch (state) {
            case sw_start:
                start = &line->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_method;
                break;

            case sw_method:
                if (ch == ' ') {
                    /* method end */
                    end = &line->data[i];
                    
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
                start = &line->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_host;
                break;

            case sw_host:
                if (ch == ':') {
                    end = &line->data[i];
                    if (end - start <= 0) {
                        log_error("http parse request line error, invalid host");
                        return RPS_ERROR;
                    }
                    string_duplicate(&req->host, (const char *)start, end - start);
                    start = &line->data[i+1]; /* cross ':' */
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
                    end = &line->data[i];
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
                start = &line->data[i];
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

                end = &line->data[i];
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
http_parse_header_line(rps_str_t *line, rps_hashmap_t *headers) {
    uint8_t c, ch;
    size_t i;
    size_t ki, vi;
    uint8_t key[HTTP_HEADER_MAX_KEY_LENGTH];
    uint8_t value[HTTP_HEADER_MAX_VALUE_LENGTH];
    
    enum {
        sw_start = 0,
        sw_key,
        sw_space_before_value,
        sw_value,
    } state;


    /* Ascii chart, copy from nginx */
   static uint8_t lowcase[] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    ki = 0;
    vi = 0;
	state = sw_start;

    for (i = 0; i < line->len; i++) {
        ch = line->data[i];

        switch (state) {
            case sw_start:

                if (ch == ' ') {
                    break;
                }
                state = sw_key;
                
                c = lowcase[ch];

                if (c) {
                    key[0] = c;
                    ki = 1;
                    break;
                }

                log_error("http parse request header error, invalid symbol in key");
                return RPS_ERROR;

            case sw_key:

                if (ki >= HTTP_HEADER_MAX_KEY_LENGTH) {
                    log_error("http parse request header error, too large key");
                    return RPS_ERROR;
                }

                c = lowcase[ch];
                
                if (c) {
                    key[ki++] = c; 
                    break;
                }

                if (ch == ':') {
                    state = sw_space_before_value;
                    break;
                }

                log_error("http parse request header error, junk in key");
                return RPS_ERROR;

            case sw_space_before_value:
                
                if (ch == ' ') {
                    break;
                }

                state = sw_value;
                
                value[0] = ch;
                vi = 1;
                break;

            case sw_value:
                if (vi >= HTTP_HEADER_MAX_VALUE_LENGTH) {
                    log_error("http parse request header error, too large value");
                    return RPS_ERROR;
                }

                value[vi++] = ch;
                break;


            default:
                break;
        }
    }

    hashmap_set(headers, key, ki, value, vi);

    return RPS_OK;

}

static rps_status_t
http_request_check(struct http_request *req) {
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


static rps_status_t
http_request_parse(uint8_t *data, size_t size) {
    size_t i, len;
    int n;
    rps_str_t line;
    struct http_request req;

    http_request_init(&req);
    
    i = 0;
    n = 0;

    for (;;) {
        string_init(&line);

        len = http_read_line(data, i, size, &line);
        if (len <= CRLF_LEN) {
            /* read empty line, only contain /r/n */
            break;
        }


        i += len;
        n++;

        printf("line <%d>: %s\n", n, line.data);

        if (n == 1) {
            if (http_parse_request_line(&line, &req) != RPS_OK) {
                log_error("parse http request line: %s error.", line.data);
                string_deinit(&line);
                return RPS_ERROR;
            }
        } else {
            if (http_parse_header_line(&line, &req.headers) != RPS_OK) {
                log_error("parse http request header line :%s error.", line.data);
                string_deinit(&line);
                return RPS_ERROR;   
            }
        }

        string_deinit(&line);
    }

    if ((size != i + 2 * CRLF_LEN) && (size != i + CRLF_LEN)) {
        log_error("http tunnel handshake contain junk: %s", data);
        /* 2*CRLF_LEN == last line \r\n\r\n */
        return RPS_ERROR;
    }
            
    if (http_request_check(&req) != RPS_OK) {
        log_error("invalid http request: %s", line.data);
        return RPS_ERROR;
    }

#ifdef RPS_DEBUG_OPEN
    http_request_dump(&req);
#endif

}

static void
http_do_handshake(struct context *ctx, uint8_t *data, size_t size) {
    rps_status_t status;

    status = http_request_parse(data, size);
    if (status != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
    }
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

