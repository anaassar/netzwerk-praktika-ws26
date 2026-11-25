#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "Socket.h"

#define ECHO_PORT   (7)
#define BUFFER_SIZE (1<<16)

//EXP: Funcion que ejecuta cada hilo para atender a un cliente.
static void *
handle_client(void *arg)
{
    int client_fd;
    ssize_t len;
    char buf[BUFFER_SIZE];

    //EXP: Recuperamos el descriptor y liberamos la memoria reservada en main().
    client_fd = *(int *)arg;
    free(arg);

    printf("Client handler started (fd=%d)\n", client_fd);
    fflush(stdout);

    //EXP: Devolvemos todo lo recibido hasta que el cliente cierre (len == 0) o haya un error (len < 0).
    while ((len = Recv(client_fd, buf, sizeof(buf), 0)) > 0) {
        //EXP: Send_all() repite send() si el kernel solo acepto una parte de los datos.
        if (Send_all(client_fd, buf, (size_t)len) < 0) {
            break;
        }
    }

    printf("Client disconnected (fd=%d)\n", client_fd);
    fflush(stdout);

    Close(client_fd);
    return NULL;
}

int
main(int argc, char **argv)
{
    int fd, client_fd, error;
    int *arg;
    int on = 1, off = 0;
    struct sockaddr_in6 server_addr;
    pthread_t thread;

    //EXP: Puerto 7 por defecto (RFC 862); opcionalmente otro puerto como argumento.
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Si un cliente se va mientras le enviamos, Send() devuelve -1 (EPIPE) en vez de matar el proceso.
    signal(SIGPIPE, SIG_IGN);

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

    server_addr.sin6_port = htons(argc == 2 ? Parse_port(argv[1]) : ECHO_PORT);

    Bind(fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr));
    Listen(fd, SOMAXCONN);

    printf("Echo server listening on port %u...\n", (unsigned)ntohs(server_addr.sin6_port));
    fflush(stdout);

    for (;;) {
        client_fd = Accept(fd, NULL, NULL);

        if (client_fd < 0) {
            continue;
        }

        //EXP: Memoria propia para cada hilo: si pasaramos &client_fd, el siguiente accept() podria sobrescribirlo.
        arg = malloc(sizeof(int));

        if (arg == NULL) {
            perror("malloc");
            Close(client_fd);
            continue;
        }

        *arg = client_fd;

        //EXP: pthread_create() devuelve el codigo de error directamente (no usa errno).
        error = pthread_create(&thread, NULL, handle_client, arg);

        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            free(arg);
            Close(client_fd);
            continue;
        }

        //EXP: Los recursos del hilo se liberan solos al terminar; no hace falta pthread_join().
        pthread_detach(thread);
    }

    Close(fd);
    return EXIT_SUCCESS;
}
