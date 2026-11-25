#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "Socket.h"

#define BUFFER_SIZE (1<<16)
#define HOST_SIZE   (1025)

//EXP: Prueba todas las direcciones IPv4 e IPv6 del host hasta que una conexion funcione.
//EXP: Devuelve el socket conectado o -1 si fallaron todas.
static int
connect_to_host(const char *host, const char *service)
{
    struct addrinfo hints, *result, *rp;
    char address[HOST_SIZE];
    int fd = -1;

    //EXP: Si el servicio es un numero (tambien con signo o espacios), comprobamos el rango: en Linux getaddrinfo() aceptaria 70000 y lo truncaria.
    if (strspn(service, "0123456789+- \t") == strlen(service)) {
        (void)Parse_port(service);
    }

    memset(&hints, 0, sizeof(hints));
    //LAB A3: Deben usarse todas las direcciones IPv4 e IPv6
    //LAB    ai_family: familia de direcciones que se busca.
    //LAB    AF_UNSPEC: cualquiera, así que getaddrinfo() devuelve tanto IPv4 como IPv6.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //EXP: Acepta nombre de host o IP, y nombre de servicio ("daytime") o numero de puerto ("13").
    //LAB A3: Use getaddrinfo(); sigue siendo posible indicar IP y número de puerto
    //LAB    host: nombre (localhost) o IP en texto (::1); getaddrinfo() reconoce los dos.
    //LAB    service: nombre (daytime, según /etc/services) o número de puerto (13).
    //LAB    &hints: IPv4 e IPv6, y SOCK_STREAM para TCP.
    //LAB    &result: lista enlazada; cada elemento ya trae dirección y puerto listos para connect().
    Getaddrinfo(host, service, &hints, &result);

    //LAB A3: Si es necesario, deben usarse todas las direcciones
    //LAB    Recorre la lista de getaddrinfo(): rp es la dirección actual y ai_next la siguiente; NULL es el final.
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        Getnameinfo(rp->ai_addr, rp->ai_addrlen,
                    address, sizeof(address), NULL, 0, NI_NUMERICHOST);

        //EXP: Aqui no usamos Socket()/Connect(): terminarian el programa al primer fallo,
        //EXP: pero nosotros queremos probar la siguiente direccion.
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (fd < 0) {
            perror("socket");
            continue;
        }

        //LAB A3: Si es necesario: solo prueba la siguiente dirección si esta falla
        //LAB    fd: socket nuevo para este intento; ai_addr, ai_addrlen: dirección con puerto y su tamaño.
        //LAB    == 0: conectó y break deja de probar; si no, se cierra fd y el bucle sigue.
        //LAB    Se usa connect() y no el wrapper Connect(), que terminaría el programa al primer fallo.
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            printf("Connected to server %s.\n", address);
            fflush(stdout);
            break;
        }

        //EXP: Tras un connect() fallido hay que crear un socket nuevo para el siguiente intento.
        fprintf(stderr, "connect to %s: %s\n", address, strerror(errno));
        Close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

int
main(int argc, char **argv)
{
    int fd, max_fd;
    int stdin_open = 1;
    fd_set readfds;
    ssize_t len;
    char buf[BUFFER_SIZE];

    //EXP: Host (nombre o IP) y servicio (nombre o numero de puerto) como argumentos.
    //LAB A3: Acepta como argumentos el nombre de host del servidor y un nombre de servicio
    //LAB    argc = 3: argv[0] es el programa, argv[1] el host (o IP) y argv[2] el servicio (o puerto).
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <service>\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Si el servidor cierra mientras enviamos, Send() devuelve -1 (EPIPE) en vez de matar el proceso.
    signal(SIGPIPE, SIG_IGN);

    //LAB A3: Usa el host y el servicio indicados
    //LAB    argv[1]: host; argv[2]: servicio.
    //LAB    fd: socket ya conectado, o -1 si no respondió ninguna dirección.
    fd = connect_to_host(argv[1], argv[2]);

    if (fd < 0) {
        fprintf(stderr, "Could not connect to %s, service %s\n", argv[1], argv[2]);
        return EXIT_FAILURE;
    }

    //EXP: El cliente termina solo cuando el servidor cierra la conexion (Recv() devuelve 0).
    for (;;) {
        //EXP: select() modifica readfds, por eso lo reconstruimos en cada vuelta.
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        max_fd = fd;

        //EXP: Despues de EOF en stdin ya no lo supervisamos (si no, select() lo marcaria siempre como listo).
        if (stdin_open) {
            FD_SET(STDIN_FILENO, &readfds);
            if (STDIN_FILENO > max_fd) {
                max_fd = STDIN_FILENO;
            }
        }

        //EXP: Espera sin timeout hasta que stdin o el socket tengan datos.
        Select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (stdin_open && FD_ISSET(STDIN_FILENO, &readfds)) {
            len = Read(STDIN_FILENO, buf, sizeof(buf));

            if (len == 0) {
                //EXP: EOF (Ctrl+D): half-close. Enviamos FIN pero seguimos recibiendo.
                printf("\nInput closed. Shutting down sending.\n");
                fflush(stdout);
                Shutdown(fd, SHUT_WR);
                stdin_open = 0;
            } else if (Send_all(fd, buf, (size_t)len) < 0) {
                break;
            }
        }

        if (FD_ISSET(fd, &readfds)) {
            len = Recv(fd, buf, sizeof(buf), 0);

            if (len < 0) {
                break;
            }

            if (len == 0) {
                //EXP: El servidor cerro su direccion de envio: no llegaran mas datos.
                printf("\nServer closed the connection.\n");
                break;
            }

            fwrite(buf, 1, (size_t)len, stdout);
            fflush(stdout);
        }
    }

    Close(fd);
    return EXIT_SUCCESS;
}
