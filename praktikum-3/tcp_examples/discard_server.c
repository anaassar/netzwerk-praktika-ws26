#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Socket.h"

#define DISCARD_PORT (9)
#define BUFFER_SIZE  (1<<16)

int
main(int argc, char **argv)
{
    int fd, client_fd, max_fd, i;
    int on = 1, off = 0;
    struct sockaddr_in6 server_addr;
    fd_set master, readfds;
    ssize_t len;
    char buf[BUFFER_SIZE];

    //EXP: Puerto 9 por defecto (RFC 863); opcionalmente otro puerto como argumento.
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Socket TCP IPv6. Con IPV6_V6ONLY = 0 acepta tambien clientes IPv4.
    fd = Socket(AF_INET6, SOCK_STREAM, 0);
    //LAB A4: El servidor admite tanto IPv4 como IPv6
    //LAB    Setsockopt(socket, nivel, opción, valor, tamaño) cambia una opción del socket:
    //LAB    fd: el socket de escucha que se configura; debe hacerse antes de Bind().
    //LAB    IPPROTO_IPV6: nivel donde está la opción; aquí, el protocolo IPv6.
    //LAB    IPV6_V6ONLY: si vale 1, el socket acepta solo IPv6; si vale 0, también IPv4.
    //LAB    &off: puntero al valor nuevo. off = 0 desactiva «solo IPv6» y por eso entran clientes IPv4.
    //LAB    sizeof(off): tamaño del valor, porque la opción se pasa como puntero genérico (void *).
    //LAB    Es necesario porque el valor por defecto depende del sistema: en FreeBSD 1, en Linux (WSL) 0.
    Setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    Setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    //EXP: memset deja sin6_addr = :: (todas las direcciones locales).
    memset(&server_addr, 0, sizeof(server_addr));
    //LAB A4: El servidor admite tanto IPv4 como IPv6: dirección en la que escucha
    //LAB    sin6_family = AF_INET6: la dirección es IPv6, igual que la familia del socket.
    //LAB    sin6_addr no se asigna: memset() la dejó a cero, y todo ceros es :: (todas las direcciones locales).
    //LAB    Escuchar en :: con IPV6_V6ONLY = 0 incluye IPv4; un cliente 127.0.0.1 llega como ::ffff:127.0.0.1.
    server_addr.sin6_family = AF_INET6;

#ifdef HAVE_SIN_LEN
    server_addr.sin6_len = sizeof(server_addr);
#endif

    server_addr.sin6_port = htons(argc == 2 ? Parse_port(argv[1]) : DISCARD_PORT);

    Bind(fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr));
    Listen(fd, SOMAXCONN);

    printf("Discard server listening on port %u...\n", (unsigned)ntohs(server_addr.sin6_port));
    fflush(stdout);

    //EXP: master contiene todos los sockets que supervisamos: al principio solo el de escucha.
    FD_ZERO(&master);
    FD_SET(fd, &master);
    max_fd = fd;

    for (;;) {
        //EXP: select() modifica el conjunto que recibe, por eso le pasamos una copia de master.
        readfds = master;
        Select(max_fd + 1, &readfds, NULL, NULL, NULL);

        for (i = 0; i <= max_fd; i++) {
            if (!FD_ISSET(i, &readfds)) {
                continue;
            }

            if (i == fd) {
                //EXP: Actividad en el socket de escucha = cliente nuevo.
                client_fd = Accept(fd, NULL, NULL);

                if (client_fd < 0) {
                    continue;
                }

                //EXP: Un fd_set solo admite descriptores menores que FD_SETSIZE.
                if (client_fd >= FD_SETSIZE) {
                    fprintf(stderr, "Too many clients: descriptor limit reached\n");
                    Close(client_fd);
                    continue;
                }

                FD_SET(client_fd, &master);

                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }

                printf("Client connected (fd=%d)\n", client_fd);
                fflush(stdout);
            } else {
                //EXP: Datos de un cliente: se leen y se descartan (RFC 863).
                len = Recv(i, buf, sizeof(buf), 0);

                if (len > 0) {
                    continue;
                }

                //EXP: len == 0: el cliente cerro la conexion; len < 0: error en esa conexion.
                printf("Client disconnected (fd=%d)\n", i);
                fflush(stdout);

                FD_CLR(i, &master);
                Close(i);

                //EXP: Si era el descriptor maximo, buscamos el nuevo maximo.
                if (i == max_fd) {
                    while (max_fd >= 0 && !FD_ISSET(max_fd, &master)) {
                        max_fd--;
                    }
                }
            }
        }
    }

    Close(fd);
    return EXIT_SUCCESS;
}
