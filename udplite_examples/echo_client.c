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


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "Socket.h"
#include "udplite_options.h"

#define BUFFER_SIZE (1 << 16)
#define MESSAGE_SIZE 9216

static int parse_coverage(const char *arg, int *coverage)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(arg, &end, 10);

    if (errno != 0 || *arg == '\0' || *end != '\0' ||
        value < 0 || value > 65535 ||
        (value > 0 && value < 8))
    {
        return -1;
    }

    *coverage = (int)value;
    return 0;
}

int main(int argc, char *argv[])
{
    int fd;
    int send_coverage, recv_coverage;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    ssize_t len;
    char buf[BUFFER_SIZE];

    if (argc != 4)
    {
        fprintf(stderr,
                "Usage: %s <server_ip> <send_coverage> <recv_coverage>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_coverage(argv[2], &send_coverage) != 0 ||
        parse_coverage(argv[3], &recv_coverage) != 0)
    {
        fprintf(stderr,
                "Coverage must be 0 or a value between 8 and 65535.\n");
        return EXIT_FAILURE;
    }

    fd = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE);

    if (setsockopt(fd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV,
                   &send_coverage, sizeof(send_coverage)) == -1)
    {
        perror("setsockopt UDPLITE_SEND_CSCOV");
        Close(fd);
        return EXIT_FAILURE;
    }

    if (setsockopt(fd, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV,
                   &recv_coverage, sizeof(recv_coverage)) == -1)
    {
        perror("setsockopt UDPLITE_RECV_CSCOV");
        Close(fd);
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1)
    {
        fprintf(stderr, "Invalid IPv4 address.\n");
        Close(fd);
        return EXIT_FAILURE;
    }

    memset(buf, 'A', MESSAGE_SIZE);

    printf("Sending with coverage %d; receiving with minimum coverage %d\n",
           send_coverage, recv_coverage);

    if (Sendto(fd, buf, MESSAGE_SIZE, 0,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr)) == -1)
    {
        perror("Sendto");
        Close(fd);
        return EXIT_FAILURE;
    }

    addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    memset(buf, 0, sizeof(buf));

    len = Recvfrom(fd, buf, sizeof(buf), 0,
                   (struct sockaddr *)&client_addr, &addr_len);

    if (len == -1)
    {
        perror("Recvfrom");
        Close(fd);
        return EXIT_FAILURE;
    }

    printf("Received %zd bytes from %s.\n",
           len, inet_ntoa(client_addr.sin_addr));

    Close(fd);
    return EXIT_SUCCESS;
}

