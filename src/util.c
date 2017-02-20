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
_rps_assert(const char *cond, const char *file, int line) {
    log_error("assert '%s' failed @ (%s, %d)", cond, file, line);
    abort();
}

int
rps_resolve_inet(const char *ip, uint16_t port, rps_addr_t *si) { 
    struct addrinfo hints;
    struct addrinfo *res, *rp;
    int status;
    char service[NI_MAXSERV];
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

    snprintf(service, sizeof(service), "%d", port);

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
    } else if (addr->family == AF_DOMAIN) {
        strcpy(name, addr->addr.name.host);
    } else {
        log_error("Unknow inet family:%d", addr->family);
        return -1;
    }

    return 0;
}

uint16_t
rps_unresolve_port(rps_addr_t *addr) {
    if (addr->family == AF_INET) {
        return ntohs(addr->addr.in.sin_port);
    } else if (addr->family == AF_INET6) {
        return ntohs(addr->addr.in6.sin6_port);
    } else if (addr->family == AF_DOMAIN) {
        return ntohs(addr->addr.name.port);
    } else {
        NOT_REACHED();
        return -1;
    }
}

void 
rps_addr_in4(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port) {
    memset(&addr->addr.in, 0, sizeof(addr->addr.in));
    addr->addr.in.sin_family = AF_INET;
    memcpy(&addr->addr.in.sin_port, port, 2);
    /* sizeof(remote.addr.in.sin_addr) == 4 */
    memcpy(&addr->addr.in.sin_addr, _addr, len);
    addr->family = AF_INET;
    addr->addrlen = sizeof(addr->addr.in);
}

void 
rps_addr_in6(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port) {
    memset(&addr->addr.in6, 0, sizeof(addr->addr.in6));
    addr->addr.in6.sin6_family = AF_INET6;
    memcpy(&addr->addr.in6.sin6_port, port, 2);
    memcpy(&addr->addr.in6.sin6_addr, _addr, len);
    addr->family = AF_INET6;
    addr->addrlen = sizeof(addr->addr.in6);
}

void 
rps_addr_name(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port) {
    memset(&addr->addr.name, 0, sizeof(addr->addr.name));
    addr->addr.name.family = AF_DOMAIN;
    memcpy(&addr->addr.name.port, port, 2);
    memcpy(&addr->addr.name.host, _addr, len); 
    addr->addr.name.host[len] = '\0';
    addr->family = AF_DOMAIN;
    addr->addrlen = sizeof(addr->addr.name);
}

