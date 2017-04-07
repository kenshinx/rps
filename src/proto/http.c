#include "http.h"
#include "core.h"
#include "util.h"
#include "b64/cdecode.h"
#include "b64/cencode.h"


#include <uv.h>


void
http_request_init(struct http_request *req) {
    req->method = http_emethod;
    string_init(&req->protocol);
    string_init(&req->host);
    req->port = 0;
    hashmap_init(&req->headers, 
            HTTP_HEADER_DEFAULT_COUNT, HTTP_HEADER_REHASH_THRESHOLD);
}

void
http_request_deinit(struct http_request *req) {
    string_deinit(&req->protocol);
    string_deinit(&req->host);
    hashmap_deinit(&req->headers);
}

void
http_request_auth_init(struct http_request_auth *auth) {
    auth->schema = http_auth_unknown;
    string_init(&auth->param);
}

void
http_request_auth_deinit(struct http_request_auth *auth) {
    string_deinit(&auth->param);
}

void
http_response_init(struct http_response *resp) {
    resp->code = http_undefine;
    string_init(&resp->body);
    string_init(&resp->status);
    string_init(&resp->protocol);
    hashmap_init(&resp->headers, 
            HTTP_HEADER_DEFAULT_COUNT, HTTP_HEADER_REHASH_THRESHOLD);
}

void 
http_response_deinit(struct http_response *resp) {
    hashmap_deinit(&resp->headers);
    string_deinit(&resp->body);
    string_deinit(&resp->status);
    string_deinit(&resp->protocol);
}

static size_t
http_read_line(uint8_t *data, size_t start, size_t end, rps_str_t *line) {
    size_t i, n, len;
    uint8_t c, last;

    ASSERT(string_empty(line));

    n = 0;
    len = 0;

    for (i=start; i<end; i++, len++) {
        c = data[i];
        if (c == LF) {
            len += 1; // make the start pointer jump current LF in last loop
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


    /* Valid ascii chart, copy from nginx */
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

rps_status_t
http_request_auth_parse(struct http_request_auth *auth, 
        uint8_t *credentials, size_t credentials_size) {
    size_t i;
    uint8_t *start, *end;
    uint8_t ch;

    enum {
        sw_start = 0,
        sw_schema,
        sw_space_before_param,
        sw_param,
        sw_end,
    } state;

    state = sw_start;

    for (i = 0;  i < credentials_size; i++) {
        ch = credentials[i];

        switch (state) {
            case sw_start:
                start = &credentials[i];

                if (ch == ' ') {
                    break;
                }

                state = sw_schema;
                break;

            case sw_schema:
                if (ch == ' ') {
                    /* end of schema token */
                    end = &credentials[i];

                    switch (end - start) {
                        case 5:
                            if (rps_str6_cmp(start, 'B', 'a', 's', 'i', 'c', ' ')) {
                                auth->schema = http_auth_basic;
                                break;
                            }
                            break;
                            
                        case 6:
                            if (rps_str6_cmp(start, 'D', 'i', 'g', 'e', 's', 't')) {
                                auth->schema = http_auth_digest;
                                break;
                            }
                            break;

                        default:
                            break;
                    }
                    
                    start = end;
                    state = sw_space_before_param;
                    break;
                }

                break;

            case sw_space_before_param:
                start = &credentials[i];

                if (ch == ' ') {
                    break;
                }

                state = sw_param;
                break;

            case sw_param:
                if (ch == ' ') {
                    state = sw_end;
                    break;
                }
                
                end = &credentials[i];
                break;

            case sw_end:
                if (ch != ' ') {
                    log_error("http prase request auth error, junk in credentials");
                    return RPS_ERROR;
                }
            
            default:
                NOT_REACHED();
        }
    }

    if (end - start <= 0) {
        log_error("http parse request auth error, invalid param");
        return RPS_ERROR;
    }

    string_duplicate(&auth->param, (const char *)start, end - start +1);

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

#ifdef HTTP_REQUEST_HEADER_MUST_CONTAIN_HOST
    if (!hashmap_has(&req->headers, "host", 4)) {
        log_error("http request check error, must have host header");
        return RPS_ERROR;
    }
#endif

    return RPS_OK;
} 

static rps_status_t
http_response_check(struct http_response *resp) {
    const char http0[] = "HTTP/1.0";
    const char http1[] = "HTTP/1.1";

    if (rps_strcmp(&resp->protocol, http0) != 0 && 
            rps_strcmp(&resp->protocol, http1) != 0) {
        log_error("http response check error, invalid http protocol: %s", 
                resp->protocol.data);
        return RPS_ERROR;
    }

    if (!http_valid_code(resp->code)) {
        log_error("http response check error, invalid http code: %d", resp->code);
        return RPS_ERROR;
    }

    return RPS_OK;
}

#ifdef RPS_DEBUG_OPEN
/* implement hashmap_iter_t */
static void
http_header_dump(void *key, size_t key_size, void *value, size_t value_size) {
    char skey[key_size + 1];
    char svalue[value_size + 1];

    memcpy(skey, key, key_size);
    memcpy(svalue, value, value_size);
    
    skey[key_size] = '\0';
    svalue[value_size] = '\0';

    log_verb("\t%s: %s", skey, svalue);
}

void
http_request_dump(struct http_request *req, uint8_t rs) {
    if (rs == http_recv) {
        log_verb("[http recv request]");
    } else {
        log_verb("[http send request]");
    }

    log_verb("\t%s %s:%d %s", http_method_str(req->method), req->host.data, 
            req->port, req->protocol.data);

    hashmap_iter(&req->headers, http_header_dump);
}

void 
http_response_dump(struct http_response *resp, uint8_t rs) {
    size_t len;
    char body[80];

    if (rs == http_recv) {
        log_verb("[http recv response]");
    } else {
        log_verb("[http send response]");
    }

    log_verb("\t%s %d %s", resp->protocol.data, resp->code, 
        resp->status.data);
    hashmap_iter(&resp->headers, http_header_dump);

    if (!string_empty(&resp->body)) {
        log_verb("");
        len = snprintf(body, 80, "%s", resp->body.data);
        if (len > 80) {
            /* body length larger than 80 bytes, 
             * show 75 character and four dots and one \0 */
            snprintf(&body[75], 5, ".....");
         }
        log_verb("\t%s", body);
    }
}
#endif


rps_status_t
http_request_parse(struct http_request *req, uint8_t *data, size_t size) {
    size_t i, len;
    int n;
    rps_str_t line;

    i = 0;
    n = 0;

    for (;;) {
        string_init(&line);

        len = http_read_line(data, i, size, &line);
        if (len <= CRLF_LEN) {
            string_deinit(&line);
            /* read empty line, only contain /r/n */
            break;
        }


        i += len;
        n++;

        if (n == 1) {
            if (http_parse_request_line(&line, req) != RPS_OK) {
                log_error("parse http request line: %s error.", line.data);
                string_deinit(&line);
                return RPS_ERROR;
            }
        } else {
            if (http_parse_header_line(&line, &req->headers) != RPS_OK) {
                log_error("parse http request header line :%s error.", line.data);
                string_deinit(&line);
                return RPS_ERROR;   
            }
        }

        string_deinit(&line);
    }

    if (i < size - 3 *CRLF_LEN) {
        log_error("http request contain junk: %s", data);
        /* 2*CRLF_LEN == last line \r\n\r\n */
        return RPS_ERROR;
    }
            
    if (http_request_check(req) != RPS_OK) {
        log_error("invalid http request: %s", data);
        return RPS_ERROR;
    }

#ifdef RPS_DEBUG_OPEN
    http_request_dump(req, http_recv);
#endif

    return RPS_OK;
}

static rps_status_t
http_parse_response_line(rps_str_t *line, struct http_response *resp) {
    uint8_t *start, *end;
    uint8_t ch;
    size_t i, len;

    enum {
        sw_start = 0,
        sw_protocol,
        sw_space_before_code,
        sw_code,
        sw_space_before_status,
        sw_status,
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

                state = sw_protocol;
                break;

            case sw_protocol:
                if (ch == ' ') {
                    end = &line->data[i];
                    len = end - start;
                    if (len <= 0) {
                        log_error("http parse response line error: invalid protocol");
                        return RPS_ERROR;
                    }
                    
                    string_duplicate(&resp->protocol, (const char *)start, end - start);

                    start = end;
                    state = sw_space_before_code;
                    break;
                }
                
                break;

            case sw_space_before_code:
                start = &line->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_code;
                break;

            case sw_code:
                if (ch >= '0' && ch <= '9') {
                    break;
                }

                if (ch == ' ') {
                    end = &line->data[i];
                    len = end - start;

                    if (len != 3) {
                        log_error("http parse response line error: invalid code");
                        return RPS_ERROR;
                    }
                    
                    for (; start < end; start++) {
                        resp->code = resp->code * 10 + (*start - '0');
                    }

                    start = end;
                    state = sw_space_before_status;
                    break;
                }

                log_error("http parse response line error: invalid code");
                return RPS_ERROR;

            case  sw_space_before_status:
                start = &line->data[i];
                if (ch == ' ') {
                    break;
                }

                state = sw_status;
                break;

            case sw_status:
                end = &line->data[i];
                break;

            case sw_end:
                if (ch != ' ') {
                    log_error("http parse respone line error: junk in response line");
                    return RPS_ERROR;
                }
                break;

            default:
                NOT_REACHED();
                
        }
    }

    if (end - start <= 0) {
        log_error("http parse response line error, invalid status");
        return RPS_ERROR;
    }
    

    len = end - start + 1;
    string_duplicate(&resp->status, (const char *)start, len);


    return RPS_OK;
}

rps_status_t
http_response_parse(struct http_response *resp, uint8_t *data, size_t size) {
    size_t i, len;
    int n;
    rps_str_t line;
    int body_start;
    int body_len;

    i = 0;
    n = 0;
    body_start = 0;
    body_len = 0;

    for (;;) {
        string_init(&line);

        len = http_read_line(data, i, size, &line);

        i += len;
        n++;

        if (len == CRLF_LEN || len == LF_LEN) {
            /* empty line, just contain /r/n/r/n or /r/n, mean body start */
            body_start = i;
            continue;
        }

        if (len == 0) {
            /* read end */
            break;
        }

        if (body_start) {
            /* free the has been read first line of body */
            string_deinit(&line); 

            body_len = size - body_start;
            if (body_len >= HTTP_BODY_MAX_LENGTH) {
                /* body too large, ignore*/
                break;
            }

            string_duplicate(&resp->body, (const char *)&data[body_start], body_len);
            
            i = i - len + body_len;
            break;
        }


        if (n == 1) {
            if (http_parse_response_line(&line, resp) != RPS_OK) {
                log_error("parse http response line error: %s", line.data);
                string_deinit(&line);
                return RPS_ERROR;
            }
        } else {
            if (http_parse_header_line(&line, &resp->headers) != RPS_OK) {
                log_error("parse http response header line error: %s", line.data);
                string_deinit(&line);
                return RPS_ERROR;
            }
        }

        string_deinit(&line);
    }

    if (i < size - 3 * CRLF_LEN) {
        log_error("http response contain junk: %s", data);
        return RPS_ERROR;
    }

    if (http_response_check(resp) != RPS_OK) {
        log_error("invalid http response: %s", data);
        return RPS_ERROR;
    }

#ifdef RPS_DEBUG_OPEN
    http_response_dump(resp, http_recv);
#endif

    return RPS_OK;
}


int
http_basic_auth(struct context *ctx, rps_str_t *param) {
    char *uname, *passwd;
    char plain[256];
    int length;
    struct server *s;
    base64_decodestate bstate;

    length = 0;

    base64_init_decodestate(&bstate);

    length = base64_decode_block((const char *)param->data, param->len, plain, &bstate);

    if (length <= 0) {
        return false;
    }

    plain[length] = '\0';

    char *delimiter = ":";

    uname = strtok(plain, delimiter);
    passwd = strtok(NULL, delimiter);

    if (uname == NULL || passwd == NULL) {
        return false;
    }

    s = ctx->sess->server;
    
    if (rps_strcmp(&s->cfg->username, uname) == 0 && 
        rps_strcmp(&s->cfg->password, passwd) == 0) {
        return true;
    }
    

    return false;
}

int 
http_basic_auth_gen(const char *uname, const char *passwd, char *output) {
    base64_encodestate bstate;   
    int length;
    char input[HTTP_HEADER_MAX_VALUE_LENGTH];

    snprintf(input, HTTP_HEADER_MAX_VALUE_LENGTH, "%s:%s", uname, passwd);
    

    length = 0;
    base64_init_encodestate(&bstate);

    length = base64_encode_block(input, strlen(input), output, &bstate);
    length += base64_encode_blockend(&output[length], &bstate);
    output[length] = '\0';

    return length;
}

static int
http_header_message(char *message, int size, struct hashmap_entry *header) {
    size_t key_size, val_size;
    int len;

    key_size = header->key_size;
    val_size = header->value_size;

    char skey[key_size + 1];
    char sval[val_size + 1];

    memcpy(skey, header->key, key_size);
    memcpy(sval, header->value, val_size);
    
    skey[key_size] = '\0';
    sval[val_size] = '\0';

    len = snprintf(message, size, "%s: %s\r\n", skey, sval);
    return len;
}

int
http_response_message(char *message, struct http_response *resp) {
    int len;
    int size;
    uint32_t i;
    struct hashmap_entry *header;

    len = 0;
    size = HTTP_MESSAGE_MAX_LENGTH;

    len += snprintf(message, size, "%s %d %s\r\n", 
            resp->protocol.data, resp->code, resp->status.data);
    
    for (i = 0; i < resp->headers.size; i++) {
        header = resp->headers.buckets[i];
        
        while (header != NULL) {
            len += http_header_message(message + len, size - len, header);
            header = header->next;
        }
    }

    len += snprintf(message + len, size - len, "\r\n\r\n");

    len += snprintf(message + len, size - len, "%s", resp->body.data);

#ifdef RPS_DEBUG_OPEN
    http_response_dump(resp, http_send);
#endif
    
    return len;
}

int 
http_request_message(char *message, struct http_request *req) {
    int len;
    int size;
    uint32_t i;
    struct hashmap_entry *header;

    len = 0;
    size = HTTP_MESSAGE_MAX_LENGTH;

    len += snprintf(message, size, "%s %s:%d %s\r\n", 
            http_method_str(req->method), req->host.data, 
            req->port, req->protocol.data);

    for (i = 0; i < req->headers.size; i++) {
        header = req->headers.buckets[i];
        
        while (header != NULL) {
            len += http_header_message(message + len, size - len, header);
            header = header->next;
        }
    }

    len += snprintf(message + len, size - len, "\r\n\r\n");

#ifdef RPS_DEBUG_OPEN
    http_request_dump(req, http_send);
#endif

    return len;

}
