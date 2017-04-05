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
        log_verb("http client authentication success.");
    } else {
        result = http_verify_fail;
        log_verb("http client authentication failed.");
    };

    http_request_auth_deinit(&auth);

next:
    
    if (result == http_verify_success) {
        remote = &ctx->sess->remote;
        rps_addr_name(remote, req.host.data, req.host.len, req.port);
        log_debug("remote: %s:%d", req.host.data, req.port);
    }

    http_request_deinit(&req);
    return result;
}

static rps_status_t
http_send_auth_require(struct context *ctx) {
    /* send http 407, auth required */
    struct http_response resp;
    size_t len;
    char body[HTTP_BODY_MAX_LENGTH];
    char message[HTTP_MESSAGE_MAX_LENGTH];
    rps_status_t status;

    http_response_init(&resp);
    
    resp.code = http_proxy_auth_required;
    
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

    hashmap_set(&resp.headers, (void *)key1, sizeof(key1), (void *)val1, v1len);

    /* set proxy-agent header*/
    const char key2[] = "Proxy-Agent";
    hashmap_set(&resp.headers, (void *)key2, sizeof(key2), 
            (void *)HTTP_DEFAULT_PROXY_AGENT, sizeof(HTTP_DEFAULT_PROXY_AGENT));

    /* set proxy-authenticate header */

    const char key3[] = "Proxy-Authenticate";
    char val3[64];
    int v3len;
    
    v3len = snprintf(val3, 64, "%s realm=\"%s\"", HTTP_DEFAULT_AUTH, HTTP_DEFAULT_REALM);
    
    hashmap_set(&resp.headers, (void *)key3, sizeof(key3), (void *)val3, v3len);

    len = http_response_message(message, &resp);
    
    ASSERT(len > 0);

    if (server_write(ctx, message, len) != RPS_OK) {
        status = RPS_ERROR;
    } else {
        status = RPS_OK;
    }

    http_response_deinit(&resp);

    return status;
}


static void
http_do_handshake(struct context *ctx) {
    int http_verify_result;

    http_verify_result = http_request_verify(ctx);

    switch (http_verify_result) {
        case http_verify_error:
            ctx->state = c_kill;
            break;
        case http_verify_success:
            ctx->state = c_exchange;
            break;
        case http_verify_fail:
            ctx->state = c_handshake_resp;
            break;
    }

    server_do_next(ctx);
}

static void
http_do_handshake_resp(struct context *ctx) {
    if (http_send_auth_require(ctx) != RPS_OK) {
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
            ctx->state = c_exchange;
            break;
        case http_verify_fail:
            ctx->state = c_auth_resp;
            break;
        case http_verify_error:
            ctx->state = c_kill;
            break;
    }

    server_do_next(ctx);
}

static void
http_do_auth_resp(struct context *ctx) {
    http_send_auth_require(ctx);
    ctx->state = c_kill;
    server_do_next(ctx);
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
            /* send http 200 */
            break;
        default:
            NOT_REACHED();
    }
}

