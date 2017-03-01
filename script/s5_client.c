#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 9890

#define MAXLINE 2024


void print_hex(char *data, int n) {
    int i;
    char buf[MAXLINE];
    va_list args;

    int len = 0;
    int size = MAXLINE;

    for (i=0; i<n; i++) {
        len += snprintf(buf + len, size - len, "%x ", data[i]);
    }
    printf("%s\n", buf);
}

void _send(int sockfd, char *data, int n) {
    write(sockfd, data, n);
    printf("send %zd bytes\n", n);
    print_hex(data, n);
}

void _recv(int sockfd) {
    int n;
    char recvBuf[MAXLINE];

    n  = read(sockfd, recvBuf, MAXLINE);
    print_hex(recvBuf, n);
}

void _readloop(int sockfd) {
    int n;
    char recvBuf[MAXLINE];

    while ((n = read(sockfd, recvBuf, MAXLINE)) > 0) {
        printf("read %zd bytes\n", n);
        printf("%s\n", recvBuf);
    }
    
    if (n < 0) {
        printf("Read error\n");
    } else {
        printf("Close connect\n");
    }
    
}

void handle(int sockfd) {
    int len;
    char sendBuf[MAXLINE];
    struct sockaddr_in sa;

    /* handshake */
    sendBuf[0] = 0x05;
    sendBuf[1] = 0x02;
    sendBuf[2] = 0x00;
    sendBuf[3] = 0x02;
    _send(sockfd, sendBuf, 4);

    _recv(sockfd);

    /* authentication */
    len = 0;
    const char *uname = "rps";
    const char *passwd = "secret";
    sendBuf[len++] = 0x01;
    sendBuf[len++] = strlen(uname);
    memcpy(&sendBuf[len], uname, strlen(uname));
    len += strlen(uname);
    sendBuf[len++] = strlen(passwd);
    memcpy(&sendBuf[len], passwd, strlen(passwd));
    len += strlen(passwd);

    //_send(sockfd, sendBuf, len);

    //_recv(sockfd);



    /* request */
    len = 0;
    sendBuf[len++] = 0x05;
    sendBuf[len++] = 0x01;
    sendBuf[len++] = 0x00;
    sendBuf[len++] = 0x01;
    
    inet_pton(AF_INET, "171.13.14.103", &sa.sin_addr);
    sa.sin_port = htons(80);
    memcpy(&sendBuf[len], &sa.sin_addr, sizeof(sa.sin_addr));
    len += sizeof(sa.sin_addr);
    memcpy(&sendBuf[len], &sa.sin_port, 2);
    len += 2;

    _send(sockfd, sendBuf, len);

    _recv(sockfd);

    /* http request */
    const char *payload = "GET / HTTP/1.1\r\nHOST: 171.13.14.103\r\n\r\n";
    printf("payload len: %lu\n", strlen(payload));
    memcpy(&sendBuf, payload, strlen(payload));
    _send(sockfd, sendBuf, strlen(payload));

    _readloop(sockfd);
    

}

int main() {
    int sockfd, connfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);    
    if (sockfd < 0) {
        printf("Create socket error\n");
        exit(1);
    } 

    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    //addr.sin_addr.s_addr =inet_addr(SERVER_HOST);
    inet_pton(AF_INET,  SERVER_HOST, &addr.sin_addr);

    connfd = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (connfd < 0) {
        printf("connect error: %s\n", strerror(connfd));
    }
    
    handle(sockfd);
    
    exit(0);
}
