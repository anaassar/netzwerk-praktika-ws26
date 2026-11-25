#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Socket.h"

//LAB A3: Servidor discard: puerto estándar 9 (RFC 863)
//LAB    Se usa si no se indica otro puerto como argumento. Es menor que 1024: necesita privilegios.
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
    //LAB A3: Servidor Discard basado en TCP
    //LAB    Socket(familia, tipo, protocolo) crea el socket y devuelve su descriptor:
    //LAB    AF_INET6: familia IPv6 (direcciones sockaddr_in6).
    //LAB    SOCK_STREAM: flujo de bytes fiable y ordenado, es decir, TCP (SOCK_DGRAM sería UDP).
    //LAB    0: protocolo por defecto para ese tipo; con SOCK_STREAM es TCP.
    fd = Socket(AF_INET6, SOCK_STREAM, 0);
    Setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    Setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    //EXP: memset deja sin6_addr = :: (todas las direcciones locales).
    memset(&server_addr, 0, sizeof(server_addr));
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
        //LAB A3: Varios clientes mediante select()
        //LAB    Un solo hilo espera a la vez al socket de escucha y a todos los clientes.
        //LAB    max_fd + 1: revisa los descriptores de 0 a max_fd.
        //LAB    &readfds: copia de master; al volver contiene solo los que están listos para leer.
        //LAB    NULL (writefds), NULL (exceptfds): no vigilamos escritura ni excepciones.
        //LAB    NULL (timeout): espera sin límite de tiempo.
        Select(max_fd + 1, &readfds, NULL, NULL, NULL);

        for (i = 0; i <= max_fd; i++) {
            if (!FD_ISSET(i, &readfds)) {
                continue;
            }

            if (i == fd) {
                //EXP: Actividad en el socket de escucha = cliente nuevo.
                //LAB A3: Varios clientes: acepta un cliente nuevo
                //LAB    Accept(fd, NULL, NULL) saca una conexión de la cola de listen() y espera si no hay ninguna:
                //LAB    fd: socket de escucha; sigue abierto para aceptar más clientes.
                //LAB    NULL, NULL: no pedimos la dirección ni el tamaño de la dirección del cliente.
                //LAB    client_fd: descriptor nuevo, solo para hablar con este cliente.
                //LAB    Solo se llama si select() marcó fd, así que normalmente no bloquea a los otros clientes.
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

                //LAB A3: Varios clientes: select() vigila también a este
                //LAB    FD_SET(descriptor, conjunto) añade client_fd a master.
                //LAB    master guarda todos los sockets vigilados; en cada vuelta se copia a readfds, que select() modifica.
                FD_SET(client_fd, &master);

                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }

                printf("Client connected (fd=%d)\n", client_fd);
                fflush(stdout);
            } else {
                //EXP: Datos de un cliente: se leen y se descartan (RFC 863).
                //LAB A3: Discard: lee los datos del cliente y los descarta
                //LAB    i: cliente que select() marcó como listo; buf, sizeof(buf): dónde leer y máximo de bytes; 0: sin opciones.
                //LAB    Si len > 0, el bucle continúa sin usar buf: eso es descartar. Si len <= 0, el cliente se fue.
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
