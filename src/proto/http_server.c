#include "http.h"


static void
http_do_handshake(struct context *ctx) {
    uint8_t *data;
    ssize_t size;
    struct http_request req;
    struct http_request_auth auth;
    struct server *s;
    rps_status_t status;

    data = (uint8_t *)ctx->rbuf;
    size = (size_t)ctx->nread;

    
    http_request_init(&req);
    
    status = http_request_parse(&req, data, size);
    if (status != RPS_OK) {
        ctx->state = c_kill;
        goto next;
    }

    s = ctx->sess->server;
    
    if (string_empty(&s->cfg->username) || string_empty(&s->cfg->password)) {
        /* rps server didn't assign username or password 
         * jump to upstream handshake phase directly. */
        ctx->state = c_exchange;
        goto next;
    }

    const char *auth_header = "proxy-authorization";
    uint8_t *credentials;
    size_t credentials_size;

    credentials = (uint8_t *)hashmap_get(&req.headers, (void *)auth_header, 
            strlen(auth_header), &credentials_size);

    if (credentials == NULL) {
        /* request header dosen't contain authorization  field 
         * jump to seend authorization request phase. */
        ctx->state = c_auth_resp;
        goto next;
    }

   
    http_request_auth_init(&auth);
    status = http_parse_request_auth(&auth, credentials, credentials_size);
    if (status != RPS_OK) {
        http_request_auth_deinit(&auth);
        ctx->state = c_kill;
        goto next;
    }

    if (auth.schema != http_auth_basic) {
        /* response 407 */
        log_warn("Only http basic authenticate supported.");
        http_request_auth_deinit(&auth);
        ctx->state = c_auth_resp;
        goto next;
    }

    if (http_basic_auth(ctx, &auth.param)) {
        ctx->state = c_exchange;
        log_verb("http client authentication success.");
    } else {
        ctx->state = c_auth_resp;
        log_verb("http client authentication failed.");
    };

    http_request_auth_deinit(&auth);

next:
    http_request_deinit(&req);
    server_do_next(ctx);
    return;
}

static void
http_do_auth(struct context *ctx) {
    /* send http 407, auth required */
    struct http_response resp;
    size_t len;
    char body[HTTP_BODY_MAX_LENGTH];
    char message[HTTP_RESPONSE_MAX_LENGTH];

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

    v1len = sprintf(val1, "%zd", len);

    hashmap_set(&resp.headers, (void *)key1, sizeof(key1), (void *)val1, v1len);

    /* set proxy-authenticate header */

    const char key2[] = "Proxy-Authenticate";
    char val2[64];
    int v2len;
    
    v2len = snprintf(val2, 64, "%s realm=\"%s\"", HTTP_DEFAULT_AUTH, HTTP_DEFAULT_REALM);
    
    hashmap_set(&resp.headers, (void *)key2, sizeof(key2), (void *)val2, v2len);

    http_response_gen(message, &resp);

}


void
http_server_do_next(struct context *ctx) {

    switch (ctx->state) {
        case c_handshake_req:
            http_do_handshake(ctx);
            break;
        case c_auth_resp:
            http_do_auth(ctx);
            break;
        default:
            NOT_REACHED();
    }
}

