#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "Socket.h"

#define BUFFER_SIZE (1<<16)

int
main(int argc, char **argv)
{
    int fd, max_fd;
    int stdin_open = 1;
    struct sockaddr_in server_addr4;
    struct sockaddr_in6 server_addr6;
    struct sockaddr *server_addr;
    socklen_t addr_len;
    unsigned short port;
    fd_set readfds;
    ssize_t len;
    char buf[BUFFER_SIZE];

    //EXP: IP del servidor (IPv4 o IPv6) y puerto como argumentos.
    //LAB A1: La IP del servidor y el puerto se pasan como parámetros de línea de comandos
    //LAB    argc: número de argumentos; argv[0] es el programa, argv[1] la IP y argv[2] el puerto.
    //LAB    Deben ser exactamente 3; si no, se muestra cómo se usa y el programa termina.
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    //EXP: Si el servidor cierra mientras enviamos, Send() devuelve -1 (EPIPE) en vez de matar el proceso.
    signal(SIGPIPE, SIG_IGN);

    //LAB A1: Puerto como parámetro
    //LAB    argv[2] es texto; Parse_port() lo convierte en número y termina si no está entre 1 y 65535.
    port = Parse_port(argv[2]);

    //EXP: Primero probamos si la direccion es IPv6, si no IPv4 (igual que inet_pton() en udp_examples).
    memset(&server_addr6, 0, sizeof(server_addr6));
    memset(&server_addr4, 0, sizeof(server_addr4));

    //LAB A1: IP del servidor como parámetro (IPv6)
    //LAB    inet_pton() convierte la IP de texto a binario («presentation to network»):
    //LAB    AF_INET6: interpreta el texto como IPv6.
    //LAB    argv[1]: IP escrita por el usuario, por ejemplo ::1.
    //LAB    &server_addr6.sin6_addr: aquí guarda los 16 bytes.
    //LAB    == 1: la conversión funcionó; 0 significa que el texto no es una IPv6.
    if (inet_pton(AF_INET6, argv[1], &server_addr6.sin6_addr) == 1) {
        server_addr6.sin6_family = AF_INET6;
#ifdef HAVE_SIN_LEN
        server_addr6.sin6_len = sizeof(server_addr6);
#endif
        server_addr6.sin6_port = htons(port);
        server_addr = (struct sockaddr *)&server_addr6;
        addr_len = sizeof(server_addr6);
        //LAB A1: IP del servidor como parámetro (IPv4)
        //LAB    (línea siguiente) Si no era IPv6, se prueba como IPv4: AF_INET y 4 bytes en sin_addr.
        //LAB    Si tampoco es IPv4, el else final muestra el error.
    } else if (inet_pton(AF_INET, argv[1], &server_addr4.sin_addr) == 1) {
        server_addr4.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
        server_addr4.sin_len = sizeof(server_addr4);
#endif
        server_addr4.sin_port = htons(port);
        server_addr = (struct sockaddr *)&server_addr4;
        addr_len = sizeof(server_addr4);
    } else {
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    //EXP: Socket TCP de la familia correcta y conexion (three-way handshake) con el servidor.
    //LAB A1: Cliente TCP
    //LAB    sa_family: AF_INET6 o AF_INET, según la IP indicada; el socket debe ser de la misma familia.
    //LAB    SOCK_STREAM: flujo de bytes fiable, es decir, TCP.
    //LAB    0: protocolo por defecto de ese tipo (TCP).
    fd = Socket(server_addr->sa_family, SOCK_STREAM, 0);
    Connect(fd, server_addr, addr_len);

    printf("Connected to server.\n");
    fflush(stdout);

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
        //LAB A1: Use select()
        //LAB    Espera a la vez a stdin y al socket; así ninguno bloquea al otro.
        //LAB    max_fd + 1: select() revisa los descriptores de 0 a max_fd.
        //LAB    &readfds: los que queremos leer; al volver contiene solo los que están listos.
        //LAB    NULL (writefds), NULL (exceptfds): no vigilamos escritura ni excepciones.
        //LAB    NULL (timeout): espera sin límite de tiempo.
        Select(max_fd + 1, &readfds, NULL, NULL, NULL);

        //LAB A1: Siempre que stdin se pueda leer
        //LAB    FD_ISSET(descriptor, conjunto): verdadero si select() lo marcó como listo.
        //LAB    STDIN_FILENO: descriptor 0, la entrada estándar (teclado).
        //LAB    stdin_open: evita leer otra vez después de EOF, porque stdin quedaría siempre «listo».
        if (stdin_open && FD_ISSET(STDIN_FILENO, &readfds)) {
            //LAB A1: ...el cliente lee los datos de stdin
            //LAB    buf, sizeof(buf): dónde guardarlos y cuántos bytes como máximo.
            //LAB    len: bytes leídos; 0 significa EOF. No bloquea, porque select() ya dijo que hay algo.
            len = Read(STDIN_FILENO, buf, sizeof(buf));

            //LAB A1: El cliente inicia el cierre cuando el usuario cierra stdin
            //LAB    Read() devuelve 0 en EOF: el usuario pulsó Ctrl+D o terminó la entrada redirigida.
            if (len == 0) {
                //EXP: EOF (Ctrl+D): half-close. Enviamos FIN pero seguimos recibiendo.
                printf("\nInput closed. Shutting down sending.\n");
                fflush(stdout);
                //LAB A1: Use shutdown(): inicia el cierre de la conexión
                //LAB    SHUT_WR cierra solo la dirección de envío y manda un FIN al servidor (half-close).
                //LAB    La recepción sigue abierta; con close() se perdería lo que el servidor aún envíe.
                Shutdown(fd, SHUT_WR);
                stdin_open = 0;
                //LAB A1: ...y los envía al servidor
                //LAB    (línea siguiente) fd: conexión con el servidor; buf: datos de stdin; len: cuántos bytes se leyeron.
                //LAB    Send_all() repite send() hasta enviarlos todos; < 0 indica que la conexión falló.
            } else if (Send_all(fd, buf, (size_t)len) < 0) {
                break;
            }
        }

        //LAB A1: Siempre que recibe datos del servidor
        //LAB    Verdadero si select() marcó el socket: hay datos o el servidor cerró la conexión.
        if (FD_ISSET(fd, &readfds)) {
            //LAB A1: ...recibe los datos del servidor
            //LAB    buf, sizeof(buf): dónde guardarlos y el máximo de bytes; 0: sin opciones especiales.
            //LAB    len > 0: bytes recibidos; 0: el servidor cerró; < 0: error.
            len = Recv(fd, buf, sizeof(buf), 0);

            if (len < 0) {
                break;
            }

            //LAB A1: El cliente termina solo cuando el servidor termina la conexión
            //LAB    recv() devuelve 0 al llegar el FIN del servidor y no quedar datos; entonces break sale del bucle.
            //LAB    Haber cerrado stdin no termina el programa: solo detuvo el envío.
            if (len == 0) {
                //EXP: El servidor cerro su direccion de envio: no llegaran mas datos.
                printf("\nServer closed the connection.\n");
                break;
            }

            //LAB A1: ...los muestra en stdout
            //LAB    buf: datos; 1: tamaño de cada elemento (1 byte); len: cuántos; stdout: salida estándar.
            //LAB    Se usa fwrite() y no printf("%s") porque los datos no acaban en '\0' y pueden contener ceros.
            fwrite(buf, 1, (size_t)len, stdout);
            fflush(stdout);
        }
    }

    Close(fd);
    return EXIT_SUCCESS;
}
