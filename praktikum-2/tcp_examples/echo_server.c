
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

// EXP: Garantiza que todos los bytes se envíen, incluso si send() solo acepta una parte.
int send_all(int socket_fd, const char *buffer, size_t length) {
    size_t total_sent = 0;

    // EXP: Repite el envío mientras queden bytes pendientes.
    while (total_sent < length) {

        // EXP: Intenta enviar únicamente los bytes que todavía faltan.
        ssize_t sent = send(socket_fd,
                            buffer + total_sent,
                            length - total_sent, 0);

        if (sent < 0) {
            // EXP: Si send() fue interrumpido por una señal, vuelve a intentarlo.
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (sent == 0) {
            return -1;
        }

        // EXP: Actualiza la cantidad de bytes enviados correctamente.
        total_sent += (size_t)sent;
    }

    return 0;
}

// EXP: Función que ejecutará cada hilo para atender a un cliente.
void *handle_client(void *arg) {

    // EXP: Recupera el descriptor del cliente recibido como argumento.
    int client_fd = *(int *)arg;

    // EXP: Libera la memoria reservada para pasar el descriptor al hilo.
    free(arg);

    char buffer[4096];

    printf("Client handler started (fd=%d)\n", client_fd);

    // EXP: Mantiene la comunicación mientras el cliente siga conectado.
    while (1) {

        // EXP: Recibe los datos enviados por el cliente.
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received == 0) {
            // EXP: recv() devuelve 0 cuando el cliente cierra su dirección de envío.
            printf("Client disconnected (fd=%d)\n", client_fd);
            break;
        }

        if (bytes_received < 0) {
            // EXP: Si recv() fue interrumpido por una señal, vuelve a intentarlo.
            if (errno == EINTR) {
                continue;
            }

            perror("recv");
            break;
        }

        // EXP: Devuelve al cliente todos los bytes recibidos, gestionando posibles envíos parciales.
        if (send_all(client_fd, buffer, (size_t)bytes_received) == -1) {
            perror("send");
            break;
        }
    }

    // EXP: Cierra el socket del cliente cuando termina su comunicación.
    close(client_fd);

    printf("Client handler finished (fd=%d)\n", client_fd);

    // EXP: Finaliza la ejecución del hilo.
    return NULL;
}

int main(int argc, char *argv[]) {

    // EXP: Utiliza el puerto 7 por defecto, correspondiente al protocolo Echo (RFC 862).
    const char *port = (argc == 2) ? argv[1] : "7";

    // EXP: Evita que el proceso termine abruptamente al enviar datos a una conexión cerrada.
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    // EXP: Inicializa la estructura de configuración.
    memset(&hints, 0, sizeof(hints));

    // EXP: Permite utilizar direcciones IPv4 o IPv6.
    hints.ai_family = AF_UNSPEC;

    // EXP: Indica que se utilizará TCP.
    hints.ai_socktype = SOCK_STREAM;

    // EXP: Configura el programa para utilizar una dirección local como servidor.
    hints.ai_flags = AI_PASSIVE;

    // EXP: Obtiene las direcciones locales disponibles para el puerto indicado.
    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int server_fd = -1;

    // EXP: Recorre las direcciones disponibles hasta conseguir crear y asociar el socket.
    for (rp = result; rp != NULL; rp = rp->ai_next) {

        // EXP: Crea el socket TCP del servidor.
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (server_fd == -1) {
            continue;
        }

        int opt = 1;

        // EXP: Permite reutilizar la dirección local cuando se reinicia el servidor.
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // EXP: Asocia el socket a una dirección y puerto local.
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        // EXP: Cierra el socket si no pudo asociarlo a esta dirección.
        close(server_fd);
        server_fd = -1;
    }

    // EXP: Libera la memoria reservada por getaddrinfo().
    freeaddrinfo(result);

    if (server_fd == -1) {
        fprintf(stderr, "Could not bind server socket\n");
        return 1;
    }

    // EXP: Pone el socket en modo escucha para aceptar conexiones entrantes.
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Echo server listening on port %s...\n", port);

    // EXP: Mantiene el servidor aceptando nuevos clientes.
    while (1) {

        // EXP: Acepta una nueva conexión y obtiene un socket para comunicarse con ese cliente.
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        // EXP: Reserva memoria para pasar el descriptor del cliente al nuevo hilo.
        int *client_arg = malloc(sizeof(int));

        if (client_arg == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client_arg = client_fd;

        pthread_t thread;

        // EXP: Crea un hilo independiente que ejecutará handle_client() para atender al cliente.
        int thread_result = pthread_create(&thread, NULL, handle_client, client_arg);

        if (thread_result != 0) {
            // EXP: pthread_create() devuelve directamente el código del error.
            fprintf(stderr, "pthread_create: %s\n", strerror(thread_result));

            free(client_arg);
            close(client_fd);
            continue;
        }

        // EXP: Desvincula el hilo para que sus recursos se liberen automáticamente cuando termine.
        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}
