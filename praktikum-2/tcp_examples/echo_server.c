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

//LAB A4: Servidor echo: puerto estándar 7 (RFC 862)
//LAB    Se usa si no se indica otro puerto como argumento. Es menor que 1024: necesita privilegios.
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
    //LAB A4: Echo: recibe los datos del cliente
    //LAB    client_fd: conexión de este cliente; buf, sizeof(buf): dónde leer y máximo de bytes; 0: sin opciones.
    //LAB    > 0: repite mientras lleguen datos; con 0 (el cliente cerró) o -1 (error) el bucle termina.
    while ((len = Recv(client_fd, buf, sizeof(buf), 0)) > 0) {
        //EXP: Send_all() repite send() si el kernel solo acepto una parte de los datos.
        //LAB A4: Echo: devuelve los mismos bytes, también si send() acepta solo una parte
        //LAB    buf, len: exactamente lo recibido; (size_t) convierte el tipo, len aquí es siempre positivo.
        //LAB    Send_all() llama a send() varias veces hasta enviar los len bytes; < 0: la conexión falló.
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
    //LAB A4: Servidor echo TCP
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
        //LAB A4: Servidor echo basado en hilos
        //LAB    pthread_create(hilo, atributos, función, argumento) crea un hilo nuevo:
        //LAB    &thread: aquí guarda el identificador del hilo.
        //LAB    NULL: atributos por defecto.
        //LAB    handle_client: función que ejecuta el hilo; atiende a un solo cliente.
        //LAB    arg: puntero con el descriptor del cliente, en memoria propia (malloc) para que otro accept() no lo cambie.
        //LAB    Se llama una vez por cada cliente aceptado, así que cada cliente tiene su hilo.
        //LAB    Devuelve 0 o un código de error (no usa errno).
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
