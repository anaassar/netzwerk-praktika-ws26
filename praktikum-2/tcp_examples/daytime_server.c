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

//LAB A2: RFC 867: el servicio daytime escucha en el puerto 13
//LAB    Se usa si no se indica otro puerto como argumento. Es menor que 1024: necesita privilegios.
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
    //LAB A2: Servidor Daytime basado en TCP
    //LAB    Socket(familia, tipo, protocolo) crea el socket y devuelve su descriptor:
    //LAB    AF_INET6: familia IPv6 (direcciones sockaddr_in6).
    //LAB    SOCK_STREAM: flujo de bytes fiable y ordenado, es decir, TCP (SOCK_DGRAM sería UDP).
    //LAB    0: protocolo por defecto para ese tipo; con SOCK_STREAM es TCP.
    fd = Socket(AF_INET6, SOCK_STREAM, 0);

    //EXP: En FreeBSD net.inet6.ip6.v6only = 1, por eso lo desactivamos explicitamente.
    Setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

    //EXP: Permite reiniciar el servidor enseguida aunque el puerto siga en TIME_WAIT.
    Setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    //EXP: memset deja sin6_addr a cero, es decir :: (in6addr_any): todas las direcciones locales.
    memset(&server_addr, 0, sizeof(server_addr));
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
        //LAB A2: RFC 867: el servidor espera conexiones TCP
        //LAB    Accept(fd, NULL, NULL) saca una conexión de la cola de listen() y espera si no hay ninguna:
        //LAB    fd: socket de escucha; sigue abierto para aceptar más clientes.
        //LAB    NULL, NULL: no pedimos la dirección ni el tamaño de la dirección del cliente.
        //LAB    client_fd: descriptor nuevo, solo para hablar con este cliente.
        client_fd = Accept(fd, NULL, NULL);

        if (client_fd < 0) {
            continue;
        }

        //EXP: Hora actual -> struct tm local -> texto legible.
        time(&now);
        localtime_r(&now, &local_time);
        //LAB A2: RFC 867: la fecha y la hora actuales como texto ASCII
        //LAB    buf, sizeof(buf): dónde escribir y el tamaño máximo.
        //LAB    %a día de la semana, %b mes, %d día, %H:%M:%S hora, %Y año, \n fin de línea.
        //LAB    &local_time: hora local calculada con time() y localtime_r().
        //LAB    len: número de caracteres escritos. RFC 867 no fija un formato concreto.
        len = strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y\n", &local_time);

        //EXP: RFC 867: enviar la fecha y hora y cerrar la conexion. Los datos del cliente se descartan.
        //LAB A2: RFC 867: al conectarse el cliente, se le envía la fecha y la hora
        //LAB    client_fd: conexión de este cliente; buf y len: el texto y su longitud.
        //LAB    No espera ninguna petición: basta con que el cliente se conecte. >= 0: se envió todo.
        if (Send_all(client_fd, buf, len) >= 0) {
            printf("Sent time to client.\n");
            fflush(stdout);

            //EXP: Enviamos FIN (el cliente ve el final), descartamos lo que llegue y despues cerramos.
            if (Shutdown(client_fd, SHUT_WR) == 0) {
                //LAB A2: RFC 867: los datos que envíe el cliente se descartan
                //LAB    drain() los lee y los ignora hasta que el cliente cierre, como máximo 1 segundo.
                //LAB    Si se cerrara con datos sin leer, TCP enviaría RST y el cliente podría perder la hora.
                drain(client_fd);
            }
        }

        //LAB A2: RFC 867: el servidor cierra la conexión después de enviar la hora
        //LAB    Cierra solo client_fd; fd sigue escuchando y el bucle acepta al siguiente cliente.
        Close(client_fd);
    }

    Close(fd);
    return EXIT_SUCCESS;
}
