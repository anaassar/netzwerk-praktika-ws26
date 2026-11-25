#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "Socket.h"

#define DAYTIME_PORT (13)
#define BUFFER_SIZE  (128)
#define DRAIN_SIZE   (4096)
#define DRAIN_MS     (1000)

//EXP: Milisegundos de un reloj que no cambia si se ajusta la hora del sistema.
static long long
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

//EXP: Lee y descarta lo que envie el cliente hasta que cierre, como maximo DRAIN_MS en total.
//EXP: Si cerraramos con datos sin leer, TCP enviaria un RST y el cliente podria perder la hora.
//EXP: El limite evita que un cliente que no cierra bloquee a los demas (servidor iterativo).
static void
drain(int client_fd)
{
    char buf[DRAIN_SIZE];
    fd_set readfds;
    struct timeval timeout;
    long long deadline, remaining;

    deadline = now_ms() + DRAIN_MS;

    while ((remaining = deadline - now_ms()) > 0) {
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        timeout.tv_sec = (time_t)(remaining / 1000);
        timeout.tv_usec = (suseconds_t)(remaining % 1000) * 1000;

        Select(client_fd + 1, &readfds, NULL, NULL, &timeout);

        //EXP: 0 = el cliente cerro (FIN); -1 = error en la conexion.
        if (FD_ISSET(client_fd, &readfds) &&
            Recv(client_fd, buf, sizeof(buf), 0) <= 0) {
            break;
        }
    }
}

int
main(int argc, char **argv)
{
    int fd, client_fd;
    int on = 1, off = 0;
    struct sockaddr_in6 server_addr;
    char buf[BUFFER_SIZE];
    time_t now;
    struct tm local_time;
    size_t len;

    //EXP: Puerto 13 por defecto (RFC 867); opcionalmente otro puerto como argumento.
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Evita que el proceso termine si escribe en una conexion ya cerrada; el error lo devuelve Send().
    signal(SIGPIPE, SIG_IGN);

    //EXP: Socket TCP IPv6. Con IPV6_V6ONLY = 0 acepta tambien clientes IPv4 (::ffff:a.b.c.d).
    fd = Socket(AF_INET6, SOCK_STREAM, 0);

    //EXP: En FreeBSD net.inet6.ip6.v6only = 1, por eso lo desactivamos explicitamente.
    //LAB A4: El servidor admite tanto IPv4 como IPv6
    //LAB    Setsockopt(socket, nivel, opción, valor, tamaño) cambia una opción del socket:
    //LAB    fd: el socket de escucha que se configura; debe hacerse antes de Bind().
    //LAB    IPPROTO_IPV6: nivel donde está la opción; aquí, el protocolo IPv6.
    //LAB    IPV6_V6ONLY: si vale 1, el socket acepta solo IPv6; si vale 0, también IPv4.
    //LAB    &off: puntero al valor nuevo. off = 0 desactiva «solo IPv6» y por eso entran clientes IPv4.
    //LAB    sizeof(off): tamaño del valor, porque la opción se pasa como puntero genérico (void *).
    //LAB    Es necesario porque el valor por defecto depende del sistema: en FreeBSD 1, en Linux (WSL) 0.
    Setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

    //EXP: Permite reiniciar el servidor enseguida aunque el puerto siga en TIME_WAIT.
    Setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    //EXP: memset deja sin6_addr a cero, es decir :: (in6addr_any): todas las direcciones locales.
    memset(&server_addr, 0, sizeof(server_addr));
    //LAB A4: El servidor admite tanto IPv4 como IPv6: dirección en la que escucha
    //LAB    sin6_family = AF_INET6: la dirección es IPv6, igual que la familia del socket.
    //LAB    sin6_addr no se asigna: memset() la dejó a cero, y todo ceros es :: (todas las direcciones locales).
    //LAB    Escuchar en :: con IPV6_V6ONLY = 0 incluye IPv4; un cliente 127.0.0.1 llega como ::ffff:127.0.0.1.
    server_addr.sin6_family = AF_INET6;

#ifdef HAVE_SIN_LEN
    server_addr.sin6_len = sizeof(server_addr);
#endif

    server_addr.sin6_port = htons(argc == 2 ? Parse_port(argv[1]) : DAYTIME_PORT);

    Bind(fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr));

    //EXP: Convierte fd en un socket de escucha; las conexiones nuevas se aceptan con Accept().
    Listen(fd, SOMAXCONN);

    printf("Daytime server listening on port %u...\n", (unsigned)ntohs(server_addr.sin6_port));
    fflush(stdout);

    //EXP: Servidor iterativo: atiende un cliente cada vez.
    for (;;) {
        //EXP: Accept() crea un socket nuevo (client_fd) solo para este cliente; fd sigue escuchando.
        client_fd = Accept(fd, NULL, NULL);

        if (client_fd < 0) {
            continue;
        }

        //EXP: Hora actual -> struct tm local -> texto legible.
        time(&now);
        localtime_r(&now, &local_time);
        len = strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y\n", &local_time);

        //EXP: RFC 867: enviar la fecha y hora y cerrar la conexion. Los datos del cliente se descartan.
        if (Send_all(client_fd, buf, len) >= 0) {
            printf("Sent time to client.\n");
            fflush(stdout);

            //EXP: Enviamos FIN (el cliente ve el final), descartamos lo que llegue y despues cerramos.
            if (Shutdown(client_fd, SHUT_WR) == 0) {
                drain(client_fd);
            }
        }

        Close(client_fd);
    }

    Close(fd);
    return EXIT_SUCCESS;
}
