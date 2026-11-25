#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "Socket.h"

//LAB A1: RFC 864: el servicio chargen escucha en el puerto 19
//LAB    Se usa si no se indica otro puerto como argumento. Es menor que 1024: necesita privilegios.
#define CHARGEN_PORT (19)
#define BUFFER_SIZE  (4096)

//EXP: RFC 864: lineas de 72 caracteres ASCII imprimibles (del espacio a '~', 95 en total) + CRLF.
#define FIRST_CHAR   (' ')
#define NUM_CHARS    (95)
#define LINE_CHARS   (72)
#define LINE_SIZE    (LINE_CHARS + 2)

//EXP: Construye una linea; offset indica con que caracter empieza.
static void
build_line(char *line, int offset)
{
    int i;

    for (i = 0; i < LINE_CHARS; i++) {
        //LAB A1: RFC 864: líneas de 72 caracteres ASCII imprimibles que van rotando
        //LAB    FIRST_CHAR: el espacio, primer carácter imprimible.
        //LAB    offset: carácter con el que empieza la línea; i: posición dentro de ella.
        //LAB    % NUM_CHARS: hay 95 caracteres; al pasar de '~' vuelve al espacio.
        line[i] = (char)(FIRST_CHAR + (offset + i) % NUM_CHARS);
    }

    line[LINE_CHARS] = '\r';
    line[LINE_CHARS + 1] = '\n';
}

//EXP: Funcion que ejecuta cada hilo para atender a un cliente.
static void *
handle_client(void *arg)
{
    int client_fd, offset = 0;
    fd_set readfds, writefds;
    char line[LINE_SIZE];
    char buf[BUFFER_SIZE];

    //EXP: Recuperamos el descriptor y liberamos la memoria reservada en main().
    client_fd = *(int *)arg;
    free(arg);

    printf("Client connected (fd=%d)\n", client_fd);
    fflush(stdout);

    for (;;) {
        //EXP: select() avisa si hay datos que descartar o si cabe otra linea por enviar.
        //EXP: Asi detectamos que el cliente se va aunque no este leyendo nuestros datos.
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(client_fd, &readfds);
        FD_SET(client_fd, &writefds);
        Select(client_fd + 1, &readfds, &writefds, NULL, NULL);

        if (FD_ISSET(client_fd, &readfds)) {
            //EXP: RFC 864: los datos recibidos se descartan. 0 = el cliente cerro; -1 = error.
            //LAB A1: RFC 864: los datos recibidos se descartan
            //LAB    Se leen en buf y no se usan.
            //LAB    <= 0: el cliente cerró (0) o la conexión falló (-1); entonces termina el hilo.
            if (Recv(client_fd, buf, sizeof(buf), 0) <= 0) {
                break;
            }
        }

        if (FD_ISSET(client_fd, &writefds)) {
            build_line(line, offset);

            //LAB A1: RFC 864: envía caracteres sin parar hasta que el cliente termina
            //LAB    line, sizeof(line): una línea completa, 72 caracteres + CRLF = 74 bytes.
            //LAB    El bucle for (;;) la repite mientras select() indique que cabe más; < 0: la conexión falló.
            if (Send_all(client_fd, line, sizeof(line)) < 0) {
                break;
            }

            //EXP: La siguiente linea empieza un caracter mas adelante.
            offset = (offset + 1) % NUM_CHARS;
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

    //EXP: Puerto 19 por defecto (RFC 864); opcionalmente otro puerto como argumento.
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Si un cliente se va mientras le enviamos, Send() devuelve -1 (EPIPE) en vez de matar el proceso.
    signal(SIGPIPE, SIG_IGN);

    //EXP: Socket TCP IPv6. Con IPV6_V6ONLY = 0 acepta tambien clientes IPv4.
    //LAB A1: Servidor chargen basado en TCP
    //LAB    Socket(familia, tipo, protocolo) crea el socket y devuelve su descriptor:
    //LAB    AF_INET6: familia IPv6 (direcciones sockaddr_in6).
    //LAB    SOCK_STREAM: flujo de bytes fiable y ordenado, es decir, TCP (SOCK_DGRAM sería UDP).
    //LAB    0: protocolo por defecto para ese tipo; con SOCK_STREAM es TCP.
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

    server_addr.sin6_port = htons(argc == 2 ? Parse_port(argv[1]) : CHARGEN_PORT);

    Bind(fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr));
    Listen(fd, SOMAXCONN);

    printf("Chargen server listening on port %u...\n", (unsigned)ntohs(server_addr.sin6_port));
    fflush(stdout);

    for (;;) {
        //LAB A1: Puede atender a varios clientes
        //LAB    Accept(fd, NULL, NULL) saca una conexión de la cola de listen() y espera si no hay ninguna:
        //LAB    fd: socket de escucha; sigue abierto para aceptar más clientes.
        //LAB    NULL, NULL: no pedimos la dirección ni el tamaño de la dirección del cliente.
        //LAB    client_fd: descriptor nuevo, solo para hablar con este cliente.
        //LAB    Está en un bucle: después de crear el hilo se acepta enseguida al siguiente cliente.
        client_fd = Accept(fd, NULL, NULL);

        if (client_fd < 0) {
            continue;
        }

        //EXP: El hilo usa select(), y un fd_set solo admite descriptores menores que FD_SETSIZE.
        if (client_fd >= (int)FD_SETSIZE) {
            fprintf(stderr, "Too many clients: descriptor limit reached\n");
            Close(client_fd);
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

        //EXP: Un hilo por cliente. pthread_create() devuelve el codigo de error (no usa errno).
        //LAB A1: Use un hilo por cliente
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
