#include "http.h"
#include "core.h"
#include "util.h"
#include "b64/cdecode.h"
#include "b64/cencode.h"


#include <uv.h>
#include <ctype.h>

const char* BYPASS_PROXY_HEADER[5] = {
    "proxy-authorization",
    "proxy-connection",
    "transfer-encoding",
    "connection",
    "upgrade"
};


void
http_request_init(struct http_request *req) {
    req->method = http_emethod;
    string_init(&req->full_uri);
    string_init(&req->schema);
    string_init(&req->host);
    req->port = 0;
    string_init(&req->path);
    string_init(&req->params);
    string_init(&req->version);
    hashmap_init(&req->headers, 
            HTTP_HEADER_DEFAULT_COUNT, HTTP_HEADER_REHASH_THRESHOLD);
}

void
http_request_deinit(struct http_request *req) {
    hashmap_deinit(&req->headers);
    string_deinit(&req->full_uri);
    string_deinit(&req->schema);
    string_deinit(&req->host);
    string_deinit(&req->path);
    string_deinit(&req->params);
    string_deinit(&req->version);
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
    string_init(&resp->version);
    hashmap_init(&resp->headers, 
            HTTP_HEADER_DEFAULT_COUNT, HTTP_HEADER_REHASH_THRESHOLD);
}

void 
http_response_deinit(struct http_response *resp) {
    hashmap_deinit(&resp->headers);
    string_deinit(&resp->body);
    string_deinit(&resp->status);
    string_deinit(&resp->version);
}

static size_t
http_read_line(uint8_t *data, size_t start, size_t end, rps_str_t *line) {
    size_t i, n, len;
    uint8_t c, last;

    ASSERT(string_empty(line));

    n = 0;
    len = 0;
    last = '\0';

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
    uint8_t *uri_start, *uri_end;
    uint8_t c, ch;
    size_t i, len;

    enum {
        sw_start = 0,
        sw_method,
        sw_space_before_uri,
        sw_space_before_host,
        sw_protocol,
        sw_schema,
        sw_schema_slash,
        sw_schema_slash_slash,
        sw_host,
        sw_port,
        sw_after_slash_in_uri,
        sw_params,
        sw_space_before_version,
        sw_version,
        sw_end,
    } state;

    state = sw_start;
    start = line->data;
    end = line->data;
    uri_start = line->data;
    uri_end = line->data;

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

                    if (rps_str4_cmp(start, 'P', 'U', 'T', ' ')) {
                        req->method = http_put;
                        break;
                    }
                   
                    break;
                case 4:
                    if (rps_str4_cmp(start, 'P', 'O', 'S', 'T')) {
                        req->method = http_post;
                        break;
                    }
                    
                    if (rps_str4_cmp(start, 'H', 'E', 'A', 'D')) {
                        req->method = http_head;
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
                if (req->method == http_connect) {
                    state = sw_space_before_host;
                } else {
                    state = sw_space_before_uri;
                }
                break;
            }

            if ((ch < 'A' || ch > 'Z') && ch != '_') {
                log_error("http parse request line error, '%s' : invalid method",  line->data);
                return RPS_ERROR;
            }
            break;

        case sw_space_before_uri:
            start = &line->data[i];
            uri_start = start;
            if (ch == ' ') {
                break;
            }

            if (ch == '/') {
                state = sw_after_slash_in_uri;
                break;
            }

            //convert character to lowercase
            c = ch | 0x20;
            if (c >= 'a' && c <= 'z') {
                state = sw_schema;
                break;
            }

            log_error("http parse request line error, '%s' : invalid uri",  line->data);
            return RPS_ERROR;

        case sw_schema:
            c = ch | 0x20;
            if (c >= 'a' && c <= 'z') {
                break;
            }

            if (ch == ':') {
                end = &line->data[i];
                if (end - start <= 0) {
                    log_error("http parse request line error, '%s' : invalid schema", line->data);
                    return RPS_ERROR;
                }
               
                string_duplicate(&req->schema, (const char *)start, end - start);
                state = sw_schema_slash;
                break;
            }

            log_error("http parse request line error, '%s' : invalid schema", line->data);
            return RPS_ERROR;

        case sw_schema_slash:
            if (ch == '/') {
                state = sw_schema_slash_slash;
                break;
            }
            log_error("http parse request line error, '%s' : invalid schema", line->data);
            return RPS_ERROR;

        case sw_schema_slash_slash:
            start = &line->data[i+1]; /* cross last '/' */
            if (ch == '/') {
                state = sw_host;
                break;
            }
            log_error("http parse request line error, '%s' : invalid schema", line->data);
            return RPS_ERROR;

        case sw_space_before_host:
            start = &line->data[i];
            uri_start = start;
            if (ch == ' ') {
                break;
            }
            state = sw_host;
            break;
            
        case sw_host:

            c = ch |0x20;
            if (c >= 'a' && c <= 'z') {
                break;
            }

            //strict host pattern
            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '=' || ch == '_') {
                break;
            }

            switch (ch) {
            case ':':
                state = sw_port;
                break;
            case '/':
                state = sw_after_slash_in_uri;
                break;
            case ' ':
                uri_end = &line->data[i];
                state = sw_space_before_version;
                break;
            default:
                log_error("http parse request line error, '%s' : invalid host", line->data);
                return RPS_ERROR;
            }

            end = &line->data[i];
            if (end - start <= 0) {
                log_error("http parse request line error, '%s' : invalid host", line->data);
                return RPS_ERROR;
            }
            string_duplicate(&req->host, (const char *)start, end - start);
            start = &line->data[i]; 
            break;

        case sw_port:
            if (ch >= '0' && ch <= '9') {
                break;
            }

            switch (ch) {
            case '/':
                state = sw_after_slash_in_uri;
                break;
            case ' ':
                uri_end = &line->data[i];
                state = sw_space_before_version;        
                break;
            default:
                log_error("http parse request line error, '%s' : invalid port", line->data);
                return RPS_ERROR;
            }

            start++; //cross ':'
            end = &line->data[i];
            len = end - start;

            if (len <=0 || len >= 6) {
                log_error("http parse request line error, '%s' : invalid port", line->data);
                return RPS_ERROR;
            }

            for (; start < end; start++) {
                req->port = req->port * 10 + (*start - '0'); 
            }

            start = end;
            break;

        case sw_after_slash_in_uri:

            if (ch != ' ' && ch != '?') {
                break;
            }

            switch (ch) {
            case ' ':
                uri_end = &line->data[i];
                state = sw_space_before_version;
                break;

            case '?':
                state = sw_params;
                break;
                
            default:
                break;
            }

            end = &line->data[i];
            if (end - start <= 0) {
                log_error("http parse request line error, '%s' : invalid path", line->data);
                return RPS_ERROR;
            }
            string_duplicate(&req->path, (const char *)start, end - start);
            start = end;
            break;

        case sw_params:
            if (ch != ' ') {
                break;
            }
            end = &line->data[i];
            if (end - start <= 0) {
                log_error("http parse request line error, '%s' : invalid params", line->data);
                return RPS_ERROR;
            }
            string_duplicate(&req->params, (const char *)start, end - start);
            uri_end = &line->data[i];
            state = sw_space_before_version;
            start = end;
            break;

        case sw_space_before_version:
            start = &line->data[i];
            if (ch == ' ') {
                break;
            }

            state = sw_version;
            break;

        case sw_version:
            if (ch == ' ') {
                state = sw_end;
                break;
            }

            end = &line->data[i];
            break;

        case sw_end:
            if (ch != ' ') {
                log_error("http parse request line error, '%s' : junk in request line", 
                        line->data);
                return RPS_ERROR;
            }
        
        default:
            NOT_REACHED();
        }
    }

    if (end - start <= 0) {
        log_error("http parse request line error, '%s' : invalid version", line->data);
        return RPS_ERROR;
    }

    string_duplicate(&req->version, (const char *)start, end - start +1);

    if (uri_end - uri_start <= 0) {
        log_error("http parse request line error, '%s' : invalid uri", line->data);
        return RPS_ERROR;
    }

    string_duplicate(&req->full_uri, (const char *)uri_start, uri_end - uri_start);

    if (state != sw_version && state != sw_end) {
        log_error("http parse request line error, '%s' : parse failed", line->data);
        return RPS_ERROR;
    }

    if (req->port == 0) {
        if (rps_strcmp(&req->schema, "https") == 0) {
            req->port = 443;
        } else {
            req->port = 80;       
        }
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

    if (ki != 0) {
        hashmap_set(headers, key, ki, value, vi);
    }

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
    start = credentials;
    end = credentials;

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

    string_duplicate(&auth->param, (const char *)start, end - start + 1);

    return RPS_OK;
}

static rps_status_t
http_request_check(struct http_request *req) {
    const char http[] = "http";
    const char https[] = "https";

    if (req->method < http_get || req->method > http_connect) {
        log_error("http request check error, invalid http method");
        return RPS_ERROR;
    }

    if (!string_empty(&req->schema)) {
        if (rps_strcmp(&req->schema, http) != 0 &&
            rps_strcmp(&req->schema, https) != 0) {
            log_error("http request check error, invalid http schema");
            return RPS_ERROR;
        }
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

    if (rps_strcmp(&resp->version, http0) != 0 && 
            rps_strcmp(&resp->version, http1) != 0) {
        log_error("http response check error, invalid http version: %s", 
                resp->version.data);
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
    if (value_size > 0) {
        memcpy(svalue, value, value_size);
    }
    
    skey[key_size] = '\0';
    svalue[value_size] = '\0';

    log_verb("\t%c%s: %s", toupper(skey[0]), skey + 1, svalue);
}

void
http_request_dump(struct http_request *req, uint8_t rs) {
    size_t len;
    char uri[60];

    if (rs == http_recv) {
        log_verb("[http recv request]");
    } else {
        log_verb("[http send request]");
    }


    if (req->method == http_connect) {
        len = snprintf(uri, 60, "%s:%d", req->host.data, req->port);
    } else {
        if (!string_empty(&req->full_uri)) {
            len = snprintf(uri, 60, "%s", req->full_uri.data);
            if (len > 60) {
                /* uri length larger than 60 bytes, 
                 * show 55 character and four dots and one \0 */
                snprintf(&uri[55], 5, ".....");
            }
        }
    }

    log_verb("\t%s %s %s", http_method_str(req->method), uri, req->version.data);
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

    log_verb("\t%s %d %s", resp->version.data, resp->code, 
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
        data[size] = '\0';
        log_error("http request contain junk: %s", data);
        /* 2*CRLF_LEN == last line \r\n\r\n */
        return RPS_ERROR;
    }
            
    if (http_request_check(req) != RPS_OK) {
        data[size] = '\0';
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
        sw_version,
        sw_space_before_code,
        sw_code,
        sw_space_before_status,
        sw_status,
        sw_end,
    } state;

    state = sw_start;
    start = line->data;
    end = line->data;

    for (i = 0; i < line->len; i++) {
        ch = line->data[i];

        switch (state) {
        case sw_start:
            start = &line->data[i];
            if (ch == ' ') {
                break;
            }

            state = sw_version;
            break;

        case sw_version:
            if (ch == ' ') {
                end = &line->data[i];
                len = end - start;
                if (len <= 0) {
                    log_error("http parse response line error: invalid version");
                    return RPS_ERROR;
                }
                
                string_duplicate(&resp->version, (const char *)start, end - start);

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

    i = 0;
    n = 0;

    for (;;) {
        string_init(&line);

        len = http_read_line(data, i, size, &line);

        i += len;
        n++;

        if (len == CRLF_LEN || len == LF_LEN) {
            /* empty line, just contain /r/n/r/n or /r/n, mean body start */
            // ignore body
            break;
        }

        if (len == 0) {
            /* read end */
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

    if (http_response_check(resp) != RPS_OK) {
        data[size] = '\0';
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

    length = snprintf(output, HTTP_HEADER_MAX_VALUE_LENGTH, "Basic ");
    length += base64_encode_block(input, strlen(input), &output[length], &bstate);
    length += base64_encode_blockend(&output[length], &bstate);

    ASSERT (length > 1);

    /* base64_encode_blockend append \n at the last of outoput, we need remove the \n */
    output[length - 1] = '\0';

    return length - 1;
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
    if (val_size > 0) {
        memcpy(sval, header->value, val_size);
    }
    
    skey[key_size] = '\0';
    sval[val_size] = '\0';

    len = snprintf(message, size, "%c%s: %s\r\n", toupper(skey[0]), skey+1, sval);
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
            resp->version.data, resp->code, resp->status.data);
    
    for (i = 0; i < resp->headers.size; i++) {
        header = resp->headers.buckets[i];
        
        while (header != NULL) {
            len += http_header_message(message + len, size - len, header);
            header = header->next;
        }
    }

    len += snprintf(message + len, size - len, "\r\n");

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

    if (req->method == http_connect) {
        len += snprintf(message, size, "%s %s:%d %s\r\n", 
                http_method_str(req->method), req->host.data, 
                req->port, req->version.data);
    } else {
        len += snprintf(message, size, "%s %s %s\r\n",
                http_method_str(req->method), req->full_uri.data,
                req->version.data);
    }


    for (i = 0; i < req->headers.size; i++) {
        header = req->headers.buckets[i];
        
        while (header != NULL) {
            len += http_header_message(message + len, size - len, header);
            header = header->next;
        }
    }

    len += snprintf(message + len, size - len, "\r\n");

#ifdef RPS_DEBUG_OPEN
    http_request_dump(req, http_send);
#endif

    return len;

}


int
http_request_verify(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    struct http_request *req;
    struct http_request_auth auth;
    struct server *s;
    rps_addr_t  *remote;
    rps_status_t status;
    int result;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    ASSERT(ctx->req == NULL);

    /* Make sure the memory be released in caller function */
    ctx->req = (struct http_request *)rps_alloc(sizeof(struct http_request));
    if (ctx->req == NULL) {
        result = http_verify_error;
        goto next;
    }
    
    http_request_init(ctx->req);
    
    status = http_request_parse(ctx->req, data, size);
    if (status != RPS_OK) {
        result = http_verify_error;
        goto next;
    }

    s = ctx->sess->server;
    req = ctx->req;
    
    if (string_empty(&s->cfg->username) || string_empty(&s->cfg->password)) {
        /* rps server didn't assign username or password 
         * jump to upstream handshake phase directly. */
        result = http_verify_success;
        goto next;
    }

    /* All HTTP header has been stored in lowercase */
    const char auth_header[] = "proxy-authorization";
    uint8_t *credentials;
    size_t credentials_size;

    credentials = (uint8_t *)hashmap_get(&req->headers, (void *)auth_header, 
            strlen(auth_header), &credentials_size);

    if (credentials == NULL) {
        /* request header dosen't contain authorization  field 
         * jump to seend authorization request phase. */
        result =  http_verify_fail;
        goto next;
    }

   
    http_request_auth_init(&auth);
    status = http_request_auth_parse(&auth, credentials, credentials_size);
    if (status != RPS_OK) {
        http_request_auth_deinit(&auth);
        result = http_verify_error;
        goto next;
    }

    if (auth.schema != http_auth_basic) {
        log_warn("Only http basic authenticate supported.");
        http_request_auth_deinit(&auth);
        result = http_verify_error;
        goto next;
    }

    if (http_basic_auth(ctx, &auth.param)) {
        result = http_verify_success;
    } else {
        result = http_verify_fail;
    };

    http_request_auth_deinit(&auth);

next:
    
    if (result == http_verify_success) {
        remote = &ctx->sess->remote;
        rps_addr_name(remote, req->host.data, req->host.len, req->port);
        log_debug("http client handshake success");
        log_debug("remote: %s:%d", req->host.data, req->port);
    }

    //http_request_deinit(ctx->req);
    return result;
}

int
http_response_verify(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    rps_status_t status;
    struct http_response resp;
    int result;
    char remoteip[MAX_INET_ADDRSTRLEN];

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    http_response_init(&resp);

    status = http_response_parse(&resp, data, size);
    if (status != RPS_OK) {
        http_response_deinit(&resp);
        log_debug("http upstream %s return invalid response", ctx->peername);
        return http_verify_error;
    }

    rps_unresolve_addr(&ctx->sess->remote, remoteip);

    /* convert http response code to rps unified reply code */
    ctx->reply_code = http_reply_code_lookup(resp.code);

    switch (resp.code) {
    case http_ok:
    case http_moved_permanently:
    case http_found:
    case http_not_modified:
        result = http_verify_success;
#ifdef RPS_DEBUG_OPEN
        log_verb("http upstream %s connect remote %s success", 
                ctx->peername, remoteip);
#endif
        break;

    case http_proxy_auth_required:
        log_debug("http upstream %s 407 authentication failed", 
                ctx->peername);
        result = http_verify_fail;
        break;

    case http_forbidden:
    case http_not_found:
    case http_server_error:
    case http_bad_gateway:
        log_debug("http upstream %s error, %d %s", ctx->peername, 
                resp.code, resp.status.data);
        result = http_verify_error;
        break;

    default:
        log_debug("http upstream %s return undefined status code, %s", 
                ctx->peername, resp.status.data);
        result = http_verify_error;
    }

    http_response_deinit(&resp);
    return result;
}


rps_status_t
http_send_request(struct context *ctx) {
    struct http_request *req;
    struct upstream *u;
    size_t i;
    char message[HTTP_MESSAGE_MAX_LENGTH];
    int len;

    req = ctx->sess->request->req;

    ASSERT(req != NULL);
    
    for (i = 0; i < BYPASS_PROXY_HEADER_LEN; i++) {
        hashmap_remove(&req->headers, (void *)BYPASS_PROXY_HEADER[i], 
                strlen(BYPASS_PROXY_HEADER[i]));
    } 

    u = &ctx->sess->upstream;
    
    if (!string_empty(&u->uname)) {
        /* autentication required */
        const char key[] = "Proxy-Authorization";
        char val[HTTP_HEADER_MAX_VALUE_LENGTH];   
        int vlen;
        
        vlen = http_basic_auth_gen((const char *)u->uname.data, 
                (const char *)u->passwd.data, val);
        hashmap_set(&req->headers, (void *)key, strlen(key), (void *)val, vlen);
    }
        
#ifdef HTTP_PROXY_CONNECTION
    /* set proxy-connection header*/
    const char key2[] = "Porxy-Connection";
    hashmap_set(&req->headers, (void *)key2, strlen(key2), 
            (void *)HTTP_DEFAULT_PROXY_CONNECTION, strlen(HTTP_DEFAULT_PROXY_CONNECTION));
#endif

#ifdef HTTP_PROXY_AGENT
    const char key3[] = "Proxy-Agent";
    hashmap_set(&req->headers, (void *)key3, strlen(key3), 
            (void *)HTTP_DEFAULT_PROXY_AGENT, strlen(HTTP_DEFAULT_PROXY_AGENT));
#endif

    if (ctx->proto == HTTP) {
        const char key4[] = "Connection";
        const char val4[] = "close";
        hashmap_set(&req->headers, (void *)key4, strlen(key4), 
            (void *)val4, strlen(val4));    
    }
    
    len = http_request_message(message, req);

    ASSERT(len > 0);

    return server_write(ctx, message, len);

}

rps_status_t
http_send_response(struct context *ctx, uint16_t code) {
    struct http_response resp;
    size_t len;
    char body[HTTP_BODY_MAX_LENGTH];
    char message[HTTP_MESSAGE_MAX_LENGTH];

    ASSERT(http_valid_code(code));

    http_response_init(&resp);
    
    resp.code = code;
    string_duplicate(&resp.status, http_resp_code_str(resp.code), strlen(http_resp_code_str(resp.code)));
    string_duplicate(&resp.version, HTTP_DEFAULT_VERSION, strlen(HTTP_DEFAULT_VERSION));
    
    /* write http body */ 
    len = snprintf(body, HTTP_BODY_MAX_LENGTH, "%d %s\n", 
            resp.code, http_resp_code_str(resp.code));

    ASSERT(len > 0);

    string_duplicate(&resp.body, body, len);

    /* set content-length header */
    const char key1[] = "Content-Length";
    char val1[32];
    int v1len;

    v1len = snprintf(val1, 32, "%zd", len);

    hashmap_set(&resp.headers, (void *)key1, strlen(key1), (void *)val1, v1len);

#ifdef HTTP_PROXY_AGENT
    /* set proxy-agent header*/
    const char key2[] = "Proxy-Agent";
    hashmap_set(&resp.headers, (void *)key2, strlen(key2), 
            (void *)HTTP_DEFAULT_PROXY_AGENT, strlen(HTTP_DEFAULT_PROXY_AGENT));
#endif


    /* set poxy authenticate required header */
    const char key3[] = "Proxy-Authenticate";
    char val3[64];
    int v3len;

    switch (code) {
    case http_proxy_auth_required:
        v3len = snprintf(val3, 64, "%s realm=\"%s\"", 
                HTTP_DEFAULT_AUTH, HTTP_DEFAULT_REALM);

        hashmap_set(&resp.headers, (void *)key3, strlen(key3), 
                (void *)val3, v3len);
        break;
    default:
        break;
    }

#ifdef HTTP_PROXY_CONNECTION
    /* set proxy-agent header*/
    const char key4[] = "Porxy-Connection";
    hashmap_set(&resp.headers, (void *)key4, strlen(key4), 
            (void *)HTTP_DEFAULT_PROXY_CONNECTION, strlen(HTTP_DEFAULT_PROXY_CONNECTION));
    
#endif


    len = http_response_message(message, &resp);
    
    ASSERT(len > 0);

    http_response_deinit(&resp);

    return server_write(ctx, message, len);
}
