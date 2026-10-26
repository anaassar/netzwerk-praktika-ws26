
#ifndef SOCKET_H
#define SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

int Socket(int domain, int type, int protocol);

int Bind(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen);

ssize_t Sendto(int sockfd,
        const void *buf,
        size_t len,
        int flags,
        const struct sockaddr *dest_addr,
        socklen_t addrlen);

ssize_t Recvfrom(int sockfd,
        void *buf,
        size_t len,
        int flags,
        struct sockaddr *src_addr,
        socklen_t *addrlen);

int Close(int fd);

#endif
