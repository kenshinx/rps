#include "http.h"


static int
http_request_verify(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    struct http_request req;
    struct http_request_auth auth;
    struct server *s;
    rps_addr_t  *remote;
    rps_status_t status;
    int result;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    
    http_request_init(&req);
    
    status = http_request_parse(&req, data, size);
    if (status != RPS_OK) {
        result = http_verify_error;
        goto next;
    }

    s = ctx->sess->server;
    
    if (string_empty(&s->cfg->username) || string_empty(&s->cfg->password)) {
        /* rps server didn't assign username or password 
         * jump to upstream handshake phase directly. */
        result = http_verify_success;
        goto next;
    }

    const char auth_header[] = "proxy-authorization";
    uint8_t *credentials;
    size_t credentials_size;

    credentials = (uint8_t *)hashmap_get(&req.headers, (void *)auth_header, 
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
        rps_addr_name(remote, req.host.data, req.host.len, req.port);
        log_debug("http client handshake success");
        log_debug("remote: %s:%d", req.host.data, req.port);
    }

    http_request_deinit(&req);
    return result;
}

static rps_status_t
http_send_response(struct context *ctx, uint16_t code) {
    struct http_response resp;
    size_t len;
    char body[HTTP_BODY_MAX_LENGTH];
    char message[HTTP_MESSAGE_MAX_LENGTH];

    ASSERT(http_valid_code(code));

    http_response_init(&resp);
    
    resp.code = code;
    string_duplicate(&resp.status, http_resp_code_str(resp.code), strlen(http_resp_code_str(resp.code)));
    string_duplicate(&resp.protocol, HTTP_DEFAULT_PROTOCOL, strlen(HTTP_DEFAULT_PROTOCOL));
    
    /* write http body */ 
    len = snprintf(body, HTTP_BODY_MAX_LENGTH, "%d %s", 
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


static void
http_do_handshake(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_request_verify(ctx);

    switch (http_verify_result) {
        case http_verify_error:
            log_verb("http client handshake error");
            ctx->state = c_kill;
            break;
        case http_verify_success:
            ctx->state = c_exchange;
            log_verb("http client handshake success");
            break;
        case http_verify_fail:
            ctx->state = c_handshake_resp;
            log_verb("http client handshake authentication required");
            break;
    }

    server_do_next(ctx);
}

static void
http_do_handshake_resp(struct context *ctx) {
    if (http_send_response(ctx, http_proxy_auth_required) != RPS_OK) {
        ctx->state = c_kill;
        server_do_next(ctx);
    } else {
        ctx->state = c_auth_req;
    }
}

static void
http_do_auth(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_request_verify(ctx);

    switch (http_verify_result) {
        case http_verify_success:
            log_debug("http client authenticate success");
            ctx->state = c_exchange;
            break;
        case http_verify_fail:
            ctx->state = c_auth_resp;
            log_debug("http client authenticate fail");
            break;
        case http_verify_error:
            ctx->state = c_kill;
            log_debug("http client authenticate error");
            break;
    }

    server_do_next(ctx);
}

static void
http_do_auth_resp(struct context *ctx) {
    http_send_response(ctx, http_proxy_auth_required);
    ctx->state = c_kill;
    server_do_next(ctx);
}

static void
http_do_reply(struct context *ctx) {
    if (ctx->reply_code == UNDEFINED_REPLY_CODE) {
        ctx->reply_code = http_server_error;
    }

    if (http_send_response(ctx, ctx->reply_code) != RPS_OK) {
        ctx->established = 0;
        ctx->state = c_kill;
        return;
    }

    if (ctx->reply_code != http_ok) {
        ctx->established = 0;
        ctx->state = c_kill;
    } else {
        ctx->established = 1;
        ctx->state = c_established;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("http client reply, connect remote %s", http_resp_code_str(ctx->reply_code));
#endif

}


void
http_server_do_next(struct context *ctx) {

    switch (ctx->state) {
        case c_handshake_req:
            http_do_handshake(ctx);
            break;
        case c_handshake_resp:
            http_do_handshake_resp(ctx);
            break;
        case c_auth_req:
            http_do_auth(ctx);
            break;
        case c_auth_resp:
            http_do_auth_resp(ctx);
            break;
        case c_reply:
            http_do_reply(ctx);
            break;
        default:
            NOT_REACHED();
    }
}

