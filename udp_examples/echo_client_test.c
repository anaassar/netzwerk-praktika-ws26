/*-
 * Copyright (c) 2013 Michael Tuexen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Socket.h"

#ifdef _WIN32
typedef int ssize_t;
typedef int socklen_t;
#endif

#define BUFFER_SIZE (1 << 16)
//#define MESSAGE_SIZE (60000)
//#define MESSAGE_SIZE (65535)
#define MESSAGE_SIZE (65507)

int main(int argc, char **argv)
{
    int fd;
    struct sockaddr_in server_addr, client_addr;
    // socklen_t addr_len;
    ssize_t len;
    char buf[BUFFER_SIZE];

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = Socket(AF_INET, SOCK_DGRAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(server_addr);
#endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7);

    if (inet_pton(AF_INET, argv[1],
                  &server_addr.sin_addr) != 1)
    {
        fprintf(stderr, "Invalid IPv4 address\n");
        Close(fd);
        exit(EXIT_FAILURE);
    }

    Connect(fd, (struct sockaddr *)&server_addr,
            sizeof(server_addr));

    memset(buf, 'A', sizeof(buf));

    Send(fd, buf, MESSAGE_SIZE, 0);

    // addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    memset(buf, 0, sizeof(buf));

    len = Recv(fd, buf, sizeof(buf), 0);

    printf("Received %zd bytes from %s.\n",
       len, inet_ntoa(server_addr.sin_addr));

    Close(fd);

    return EXIT_SUCCESS;
}
