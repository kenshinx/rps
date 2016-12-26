#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include <uv.h>

#include "util.h"
#include "log.h"

void *
_rps_alloc(size_t size, const char *name, int line) {
    void *p;
    
    ASSERT(size != 0);

    p = malloc(size);
    
    if (p == NULL) {
        log_error("malloc(%zu) failed @ %s:%d", size, name, line);
    } else {
        log_verb("malloc(%zu) at %p @ %s:%d", size, p, name, line);
    }

    return p;
}

void *
_rps_zalloc(size_t size, const char *name, int line) {
    void *p;

    p = _rps_alloc(size, name, line);\
    if (p != NULL) {
        memset(p, 0, size);
    }

    return p;
}

void *
_rps_calloc(size_t nmemb, size_t size, const char *name, int line) {
    return _rps_alloc(nmemb * size, name, line);
}

void *
_rps_realloc(void *ptr, size_t size, const char *name, int line) {
    void *p;
    
    ASSERT(size != 0);

    p = realloc(ptr, size);
    
    if (p == NULL) {
        log_error("realloc(%zu) failed @ %s:%d", size, name, line);
    } else {
        log_verb("realloc(%zu) at %p @ %s:%d", size, p, name, line);
    }
    
    return p;
}

void 
_rps_free(void *ptr, const char *name, int line) {
    ASSERT(ptr != NULL);
    log_verb("free(%p) @ %s:%d", ptr, name, line);
    free(ptr);
}

void
rps_assert(const char *cond, const char *file, int line) {
    log_error("assert '%s' failed @ (%s, %d)", cond, file, line);
    abort();
}

int
rps_resolve_inet(const char *ip, uint16_t port, rps_addr_t *si) { 
    struct addrinfo hints;
    struct addrinfo *res, *rp;
    int status;
    char service[16];
    bool found;

    ASSERT(rps_valid_port(port));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    sprintf(service, "%d", port);

    status = getaddrinfo(ip, service, &hints, &res); 
    if (status != 0) {
        log_error("get address info %s:%s failed: %s", 
                ip, service, gai_strerror(status));
        return -1;
    }

    for (rp = res, found = false; rp != NULL; rp = rp->ai_next) {
        si->family = rp->ai_family;
        si->addrlen = rp->ai_addrlen;
        memcpy(&si->addr, rp->ai_addr, si->addrlen);
        found = true;
        break;
    }

    freeaddrinfo(res);  

    return !found ? -1 : 0;
}


int 
rps_unresolve_addr(rps_addr_t *addr, char *name) {
    int err;
    
    if (addr->family == AF_INET) {
        err = uv_ip4_name((struct sockaddr_in *)&addr->addr, name, INET_ADDRSTRLEN);
        if (err) {
            UV_SHOW_ERROR(err, "uv_ip4_name");
            return -1;
        }
    } else if (addr->family == AF_INET6) {
        err = uv_ip6_name((struct sockaddr_in6 *)&addr->addr, name, INET6_ADDRSTRLEN);
        if (err) {
            UV_SHOW_ERROR(err, "uv_ip6_name");
            return -1;
        }
    } else {
        log_error("Unknow inet family:%d", addr->family);
        return -1;
    }

    return 1;
}
