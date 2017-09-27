#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

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
    }
        
#ifdef  RPS_MORE_VERBOSE
    log_verb("malloc(%zu) at %p @ %s:%d", size, p, name, line);
#endif
    

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
        return NULL;
    }

#ifdef  RPS_MORE_VERBOSE
    log_verb("realloc(%zu) at %p @ %s:%d", size, p, name, line);
#endif
    
    return p;
}


void 
#ifdef RPS_MORE_VERBOSE
_rps_free(void *ptr, const char *name, int line) {
    log_verb("free(%p) @ %s:%d", ptr, name, line);
#else
_rps_free(void *ptr) {
#endif

    ASSERT(ptr != NULL);
    free(ptr);
}

void
_rps_assert(const char *cond, const char *file, int line) {
    log_error("assert '%s' failed @ (%s, %d)", cond, file, line);
    abort();
}

int
rps_resolve_inet(const char *node, uint16_t port, rps_addr_t *si) { 
    struct addrinfo hints;
    struct addrinfo *res, *rp;
    int status;
    char service[NI_MAXSERV];
    bool found;

    ASSERT(rps_valid_port(port));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    snprintf(service, sizeof(service), "%d", port);

    status = getaddrinfo(node, service, &hints, &res); 
    if (status != 0) {
        log_error("get address info %s:%s failed: %s", 
                node, service, gai_strerror(status));
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
        return addr->addr.name.port;
    } else {
        log_error("Unknow inet family:%d", addr->family);
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
rps_addr_name(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint16_t port) {
    ASSERT(len < MAX_HOSTNAME_LEN);
    memset(&addr->addr.name, 0, sizeof(addr->addr.name));
    addr->addr.name.family = AF_DOMAIN;
    memcpy(&addr->addr.name.host, _addr, len); 
    addr->addr.name.host[len] = '\0';
    addr->addr.name.port = port;
    addr->family = AF_DOMAIN;
    addr->addrlen = sizeof(addr->addr.name);
}

void
rps_init_random() {
    srand(time(NULL));
}


int 
rps_random(int max) {
    ASSERT(max > 0);
    return rand() %max;
}


static char *
_rps_safe_utoa(int _base, uint64_t val, char *buf)
{
    char hex[] = "0123456789abcdef";
    uint32_t base = (uint32_t) _base;
    *buf-- = 0;
    do {
        *buf-- = hex[val % base];
    } while ((val /= base) != 0);
    return buf + 1;
}

static char *
_rps_safe_itoa(int base, int64_t val, char *buf)
{
    char hex[] = "0123456789abcdef";
    char *orig_buf = buf;
    const int32_t is_neg = (val < 0);
    *buf-- = 0;

    if (is_neg) {
        val = -val;
    }
    if (is_neg && base == 16) {
        int ix;
        val -= 1;
        for (ix = 0; ix < 16; ++ix)
            buf[-ix] = '0';
    }

    do {
        *buf-- = hex[val % base];
    } while ((val /= base) != 0);

    if (is_neg && base == 10) {
        *buf-- = '-';
    }

    if (is_neg && base == 16) {
        int ix;
        buf = orig_buf - 1;
        for (ix = 0; ix < 16; ++ix, --buf) {
            /* *INDENT-OFF* */
            switch (*buf) {
            case '0': *buf = 'f'; break;
            case '1': *buf = 'e'; break;
            case '2': *buf = 'd'; break;
            case '3': *buf = 'c'; break;
            case '4': *buf = 'b'; break;
            case '5': *buf = 'a'; break;
            case '6': *buf = '9'; break;
            case '7': *buf = '8'; break;
            case '8': *buf = '7'; break;
            case '9': *buf = '6'; break;
            case 'a': *buf = '5'; break;
            case 'b': *buf = '4'; break;
            case 'c': *buf = '3'; break;
            case 'd': *buf = '2'; break;
            case 'e': *buf = '1'; break;
            case 'f': *buf = '0'; break;
            }
            /* *INDENT-ON* */
        }
    }
    return buf + 1;
}


static const char *
_rps_safe_check_longlong(const char *fmt, int32_t * have_longlong)
{
    *have_longlong = false;
    if (*fmt == 'l') {
        fmt++;
        if (*fmt != 'l') {
            *have_longlong = (sizeof(long) == sizeof(int64_t));
        } else {
            fmt++;
            *have_longlong = true;
        }
    }
    return fmt;
}

int
_rps_safe_vsnprintf(char *to, size_t size, const char *format, va_list ap)
{
    int64_t ival = 0;
    uint64_t uval = 0;
    char *start = to;
    char *end = start + size - 1;
    for (; *format; ++format) {
        int32_t have_longlong = false;
        if (*format != '%') {
            if (to == end) {    /* end of buffer */
                break;
            }
            *to++ = *format;    /* copy ordinary char */
            continue;
        }
        ++format;               /* skip '%' */

        format = _rps_safe_check_longlong(format, &have_longlong);

        switch (*format) {
        case 'd':
        case 'i':
        case 'u':
        case 'x':
        case 'p':
            {
                if (*format == 'p')
                    have_longlong = (sizeof(void *) == sizeof(uint64_t));
                if (have_longlong) {
                    if (*format == 'u') {
                        uval = va_arg(ap, uint64_t);
                    } else {
                        ival = va_arg(ap, int64_t);
                    }
                } else {
                    if (*format == 'u') {
                        uval = va_arg(ap, uint32_t);
                    } else {
                        ival = va_arg(ap, int32_t);
                    }
                }

                {
                    char buff[22];
                    const int base = (*format == 'x' || *format == 'p') ? 16 : 10;

                    /* *INDENT-OFF* */
                    char *val_as_str = (*format == 'u') ?
                        _rps_safe_utoa(base, uval, &buff[sizeof(buff) - 1]) :
                        _rps_safe_itoa(base, ival, &buff[sizeof(buff) - 1]);
                    /* *INDENT-ON* */

                    /* Strip off "ffffffff" if we have 'x' format without 'll' */
                    if (*format == 'x' && !have_longlong && ival < 0) {
                        val_as_str += 8;
                    }

                    while (*val_as_str && to < end) {
                        *to++ = *val_as_str++;
                    }
                    continue;
                }
            }
        case 's':
            {
                const char *val = va_arg(ap, char *);
                if (!val) {
                    val = "(null)";
                }
                while (*val && to < end) {
                    *to++ = *val++;
                }
                continue;
            }
        }
    }
    *to = 0;
    return (int)(to - start);
}

int
_rps_safe_snprintf(char *to, size_t n, const char *fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);
    result = _rps_safe_vsnprintf(to, n, fmt, args);
    va_end(args);
    return result;
}
