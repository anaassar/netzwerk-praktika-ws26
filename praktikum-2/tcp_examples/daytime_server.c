#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

//EXP: Envía todos los bytes del mensaje, incluso si send() no consigue enviarlos todos en una sola llamada.
int send_all(int socket_fd, const char *buffer, size_t length) {
    size_t total_sent = 0;

    //EXP: Continúa enviando mientras queden bytes pendientes.
    while (total_sent < length) {

        //EXP: Intenta enviar los bytes restantes al cliente mediante el socket TCP.
        ssize_t sent = send(socket_fd, buffer + total_sent,
                            length - total_sent, 0);

        //EXP: Comprueba si send() produjo un error.
        if (sent < 0) {

            //EXP: Si la llamada fue interrumpida por una señal, vuelve a intentarlo.
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        //EXP: Si send() devuelve 0, no se pudo avanzar en el envío.
        if (sent == 0) {
            return -1;
        }

        //EXP: Actualiza la cantidad de bytes que ya fueron enviados correctamente.
        total_sent += (size_t)sent;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    //EXP: Utiliza el puerto 13 por defecto, correspondiente al protocolo Daytime (RFC 867).
    const char *port = (argc == 2) ? argv[1] : "13";

    //EXP: Evita que el proceso termine automáticamente si intenta enviar datos a un socket cerrado.
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    //EXP: Inicializa la estructura con ceros para configurar únicamente las opciones necesarias.
    memset(&hints, 0, sizeof(hints));

    //EXP: Permite utilizar direcciones IPv4 o IPv6.
    hints.ai_family = AF_UNSPEC;

    //EXP: Indica que se utilizará TCP (socket orientado a conexión).
    hints.ai_socktype = SOCK_STREAM;

    //EXP: Indica que el programa actuará como servidor y utilizará una dirección local.
    hints.ai_flags = AI_PASSIVE;

    //EXP: Obtiene las direcciones locales compatibles con el puerto solicitado.
    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int server_fd = -1;

    //EXP: Recorre las direcciones disponibles hasta conseguir crear y asociar un socket correctamente.
    for (rp = result; rp != NULL; rp = rp->ai_next) {

        //EXP: Crea un socket utilizando la familia de direcciones y el protocolo indicados por getaddrinfo().
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (server_fd == -1) {
            continue;
        }

        int opt = 1;

        //EXP: Permite reutilizar la dirección local después de cerrar el servidor, evitando ciertos problemas al reiniciarlo.
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        //EXP: Asocia el socket a una dirección y puerto local.
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        //EXP: Cierra el socket si no fue posible asociarlo a esta dirección antes de probar la siguiente.
        close(server_fd);
        server_fd = -1;
    }

    //EXP: Libera la memoria reservada por getaddrinfo(), ya que las direcciones obtenidas ya no son necesarias.
    freeaddrinfo(result);

    //EXP: Comprueba si fue posible crear y asociar el socket del servidor.
    if (server_fd == -1) {
        fprintf(stderr, "Could not bind server socket\n");
        return 1;
    }

    //EXP: Convierte el socket en un socket de escucha para aceptar conexiones TCP entrantes.
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Daytime server listening on port %s...\n", port);

    //EXP: Mantiene el servidor funcionando para aceptar múltiples conexiones consecutivas.
    while (1) {

        //EXP: Espera una conexión entrante y crea un nuevo socket para comunicarse con ese cliente.
        int client_fd = accept(server_fd, NULL, NULL);

        //EXP: Si accept() fue interrumpido por una señal, vuelve a esperar una conexión.
        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        time_t now;

        //EXP: Obtiene la fecha y hora actuales del sistema.
        time(&now);

        struct tm local_time;
        char time_buffer[128];

        //EXP: Convierte el tiempo obtenido a una estructura con sus componentes de fecha y hora locales.
        localtime_r(&now, &local_time);

        //EXP: Formatea la fecha y hora como texto legible para enviarlas al cliente.
        strftime(time_buffer, sizeof(time_buffer),
                 "%a %b %d %H:%M:%S %Y\n", &local_time);

        //EXP: Envía la fecha y hora al cliente utilizando la función que garantiza el envío de todos los bytes.
        if (send_all(client_fd, time_buffer, strlen(time_buffer)) == -1) {
            perror("send");
        } else {
            printf("Sent time to client.\n");
        }

        //EXP: Cierra la conexión con el cliente después de enviarle la fecha y hora.
        close(client_fd);
    }

    //EXP: Cierra el socket principal del servidor (esta instrucción no se alcanza mientras el bucle sea infinito).
    close(server_fd);

    return 0;
}