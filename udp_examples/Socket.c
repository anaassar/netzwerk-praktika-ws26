
#include <sys/types.h>
#include <io.h>

#include "Socket.h"

#include <stdio.h>
#include <stdlib.h>

int Socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);

    if (fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int Bind(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen)
{
    if (bind(sockfd, addr, addrlen) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    return 0;
}


ssize_t Sendto(int sockfd,
            const void *buf,
            size_t len,
            int flags,
            const struct sockaddr *dest_addr,
            socklen_t addrlen)
{
    ssize_t result = sendto(sockfd, buf, len, flags,
                            dest_addr, addrlen);

    if (result < 0)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    return result;
}

ssize_t Recvfrom(int sockfd,
                void *buf,
                size_t len,
                int flags,
                struct sockaddr *src_addr,
                socklen_t *addrlen)
{
    ssize_t result;
    result = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

    if (result < 0)
    {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    return result;
}

int Close(int fd)
{
    if (close(fd) < 0)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
