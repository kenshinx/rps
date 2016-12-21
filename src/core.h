#ifndef _RPS_CORE_H
#define _RPS_CORE_H

#define RPS_OK      0
#define RPS_ERROR   -1
#define RPS_ENOMEM  -2

typedef int rps_status_t;

/*
 * socks5 proxy:
 * https://www.ietf.org/rfc/rfc1928.txt
 *
 * http tunnel proxy:
 * https://tools.ietf.org/html/draft-luotonen-web-proxy-tunneling-01
 *
 */
typedef enum {
    SOCKS5,
    HTTP,

#ifdef SOCKS4_PROXY_SUPPORT
    SOCKS4,
#endif

} rps_proxy_t;

#endif
