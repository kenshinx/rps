#ifndef _RPG_CORE_H
#define _RPG_CORE_H

#define RPG_OK      0
#define RPG_ERROR   -1
#define RPG_ENOMEM  -2

typedef int rpg_status_t;

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

} rpg_proxy_t;

#endif
