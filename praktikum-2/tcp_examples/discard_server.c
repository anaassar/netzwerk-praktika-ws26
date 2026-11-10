#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

int main(int argc, char *argv[]) {

    //EXP: Utiliza el puerto 9 por defecto, correspondiente al protocolo Discard (RFC 863).
    const char *port = (argc == 2) ? argv[1] : "9";

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    //EXP: Inicializa la estructura con ceros antes de configurar sus opciones.
    memset(&hints, 0, sizeof(hints));

    //EXP: Permite utilizar direcciones IPv4 o IPv6.
    hints.ai_family = AF_UNSPEC;

    //EXP: Indica que se utilizará TCP.
    hints.ai_socktype = SOCK_STREAM;

    //EXP: Indica que el programa funcionará como servidor y utilizará una dirección local.
    hints.ai_flags = AI_PASSIVE;

    //EXP: Obtiene las direcciones locales disponibles para el puerto indicado.
    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int server_fd = -1;

    //EXP: Recorre las direcciones disponibles hasta encontrar una en la que pueda crear y asociar el socket.
    for (rp = result; rp != NULL; rp = rp->ai_next) {

        //EXP: Crea el socket TCP del servidor.
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (server_fd == -1) {
            continue;
        }

        int opt = 1;

        //EXP: Permite reutilizar la dirección local cuando se reinicia el servidor.
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        //EXP: Asocia el socket a una dirección y puerto local.
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        //EXP: Cierra el socket si no pudo asociarlo a esta dirección.
        close(server_fd);
        server_fd = -1;
    }

    //EXP: Libera la memoria reservada por getaddrinfo().
    freeaddrinfo(result);

    if (server_fd == -1) {
        fprintf(stderr, "Could not bind server socket\n");
        return 1;
    }

    //EXP: Pone el socket en modo escucha para aceptar conexiones TCP entrantes.
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Discard server listening on port %s...\n", port);

    fd_set master;
    fd_set readfds;

    //EXP: Inicializa el conjunto principal de descriptores supervisados.
    FD_ZERO(&master);

    //EXP: Añade el socket del servidor para detectar nuevas conexiones.
    FD_SET(server_fd, &master);

    //EXP: Guarda el descriptor más alto para utilizarlo como límite en select().
    int max_fd = server_fd;

    char buffer[4096];

    //EXP: Mantiene el servidor funcionando mientras atiende a múltiples clientes.
    while (1) {

        //EXP: Copia el conjunto principal porque select() modifica el conjunto que recibe.
        readfds = master;

        //EXP: Espera hasta que alguno de los sockets esté listo para realizar una operación de lectura.
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity == -1) {

            //EXP: Si select() fue interrumpido por una señal, vuelve a esperar.
            if (errno == EINTR) {
                continue;
            }

            perror("select");
            break;
        }

        //EXP: Recorre los descriptores para identificar cuáles tienen actividad.
        for (int fd = 0; fd <= max_fd; fd++) {

            //EXP: Comprueba si el descriptor actual fue marcado como listo para lectura por select().
            if (FD_ISSET(fd, &readfds)) {

                //EXP: Si la actividad ocurrió en el socket principal, significa que hay un nuevo cliente.
                if (fd == server_fd) {

                    //EXP: Acepta la nueva conexión y obtiene un socket exclusivo para comunicarse con ese cliente.
                    int client_fd = accept(server_fd, NULL, NULL);

                    if (client_fd == -1) {
                        if (errno == EINTR) {
                            continue;
                        }

                        perror("accept");
                        continue;
                    }

                    //EXP: Comprueba que el descriptor pueda incluirse en un conjunto fd_set.
                    if (client_fd >= FD_SETSIZE) {
                        fprintf(stderr, "Too many clients: descriptor limit reached\n");
                        close(client_fd);
                        continue;
                    }

                    //EXP: Añade el nuevo cliente al conjunto de sockets que select() debe supervisar.
                    FD_SET(client_fd, &master);

                    //EXP: Actualiza el descriptor máximo si el nuevo cliente tiene un valor mayor.
                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }

                    printf("Client connected (fd=%d)\n", client_fd);

                } else {

                    //EXP: Recibe los datos enviados por un cliente conectado.
                    ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);

                    if (bytes_received == 0) {

                        //EXP: Si recv() devuelve 0, el cliente cerró su conexión.
                        printf("Client disconnected (fd=%d)\n", fd);

                        //EXP: Elimina el descriptor del conjunto de sockets supervisados.
                        FD_CLR(fd, &master);

                        //EXP: Cierra el socket del cliente y libera el descriptor.
                        close(fd);

                        //EXP: Si se cerró el descriptor máximo, busca el nuevo descriptor máximo.
                        if (fd == max_fd) {
                            while (max_fd >= 0 && !FD_ISSET(max_fd, &master)) {
                                max_fd--;
                            }
                        }

                    } else if (bytes_received < 0) {

                        //EXP: Si recv() fue interrumpido por una señal, continúa con el siguiente ciclo.
                        if (errno == EINTR) {
                            continue;
                        }

                        perror("recv");

                        //EXP: Elimina del conjunto el cliente cuya recepción produjo un error.
                        FD_CLR(fd, &master);

                        //EXP: Cierra la conexión del cliente que produjo el error.
                        close(fd);

                        //EXP: Recalcula el descriptor máximo si fue necesario.
                        if (fd == max_fd) {
                            while (max_fd >= 0 && !FD_ISSET(max_fd, &master)) {
                                max_fd--;
                            }
                        }
                    }

                    //EXP: Si bytes_received es mayor que 0, los datos se descartan simplemente sin procesarlos ni responder al cliente.
                }
            }
        }
    }

    //EXP: Cierra todas las conexiones que todavía permanecen abiertas.
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &master)) {
            close(fd);
        }
    }

    return 0;
}