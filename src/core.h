#ifndef _RPS_CORE_H
#define _RPS_CORE_H

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2

typedef int rps_status_t;

typedef enum {
    SOCKS5,
    HTTP,

#ifdef SOCKS4_PROXY_SUPPORT
    SOCKS4,
#endif

#ifdef PRIVATE_PROXY_SUPPORT
    PRIVATE,
#endif

} rps_proxy_t;

#endif
