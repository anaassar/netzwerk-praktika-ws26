#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <netdb.h>

int send_all(int socket_fd, const char *buffer, size_t length) // EXP: TCP trabaja con un flujo de bytes y send() no garantiza que envíe toda la cantidad solicitada en una única llamada.
{
    size_t total = 0;

    while (total < length)
    {
        ssize_t sent = send(
            socket_fd,
            buffer + total,
            length - total,
            0);

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (sent == 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) //EXP: Comprueba que el usuario haya proporcionado la dirección IP del servidor y el puerto como argumentos.
    {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN); // EXP: evita que el programa termine automáticamente si intenta enviar datos a una conexión que ya se ha cerrado. Así podemos gestionar el error nosotros mismos.

    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // EXP: Indica que queremos un socket orientado a flujo, es decir, TCP.

    int status = getaddrinfo( //EXP: Obtiene las direcciones posibles del servidor a partir de la IP o nombre de host y el puerto.
        argv[1],
        argv[2],
        &hints,
        &result);

    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    int socket_fd = -1;

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        socket_fd = socket( //EXP: Crea un socket TCP utilizando los parámetros de la dirección obtenida.
            rp->ai_family,
            rp->ai_socktype,
            rp->ai_protocol);

        if (socket_fd == -1)
        {
            continue;
        }

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0) //EXP: Establece la conexión TCP con el servidor.
        {
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(result);

    if (socket_fd == -1)
    {
        perror("Could not connect");
        return EXIT_FAILURE;
    }

    printf("Connected to server.\n");

    char buffer[4096];

    int stdin_open = 1;
    int server_open = 1;

    while (server_open)
    {

        fd_set readfds; // EXP: conjunto de descriptores que queremos supervisar.

        FD_ZERO(&readfds); // EXP: Limpia el conjunto para comenzar una nueva supervisión.

        FD_SET(socket_fd, &readfds); // EXP: Añade el socket TCP porque queremos saber cuándo llegan datos del servidor.

        if (stdin_open)
        {
            FD_SET(STDIN_FILENO, &readfds); // EXP: Añade la entrada estándar porque queremos saber cuándo el usuario escribe algo.
        }

        int max_fd = socket_fd;

        if (stdin_open && STDIN_FILENO > max_fd)
        {
            max_fd = STDIN_FILENO;
        }

        int activity = select(  //EXP: Espera hasta que haya actividad de lectura en alguno de los descriptores supervisados.
            max_fd + 1,
            &readfds,
            NULL,  // EXP: No supervisamos escritura.
            NULL,  // EXP: No supervisamos excepciones.
            NULL); // EXP: 	No establecemos timeout: esperamos indefinidamente.

        if (activity < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("select");
            break;
        }

        if (stdin_open && FD_ISSET(STDIN_FILENO, &readfds))
        {

            ssize_t bytes_read = read( //EXP: Lee los datos introducidos por el usuario desde la entrada estándar.
                STDIN_FILENO,
                buffer,
                sizeof(buffer));

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("read");
                break;
            }

            if (bytes_read == 0) // EXP: significa que hemos llegado al final de la entrada (End Of File / EOF).
            {

                printf("\nInput closed. Shutting down sending.\n");

                if (shutdown(socket_fd, SHUT_WR) < 0) // EXP: SHUT_WRITE Desactivamos el canal de envío del socket. Pero NO cerramos el socket completo.
                {
                    perror("shutdown");
                }

                stdin_open = 0;
            }
            else
            {

                if (send_all(socket_fd, buffer, (size_t)bytes_read) < 0) //EXP: Envía todos los datos leídos al servidor, incluso si send() solo acepta una parte en una llamada.
                {
                    perror("send");
                    break;
                }
            }
        }

        if (FD_ISSET(socket_fd, &readfds))
        {

            ssize_t bytes_received = recv( //EXP: Recibe los datos enviados por el servidor a través de la conexión TCP.
                socket_fd,
                buffer,
                sizeof(buffer),
                0);

            if (bytes_received < 0) // EXP: Se produjo un error
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("recv");
                break;
            }

            if (bytes_received == 0) //EXP: Detecta que el servidor ha cerrado su dirección de envío y que no quedan más datos por recibir.
            {

                printf("\nServer closed the connection.\n");

                server_open = 0;
            }
            else // EXP: Hemos recibido datos
            {

                fwrite(buffer, 1, (size_t)bytes_received, stdout);
                fflush(stdout);
            }
        }
    }

    close(socket_fd); //EXP: Cierra el socket y libera el descriptor de archivo utilizado para la conexión TCP.

    return EXIT_SUCCESS;
}