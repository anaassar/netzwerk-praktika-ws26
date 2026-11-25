#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Socket.h"

//EXP: Espacio suficiente para una direccion numerica con zona (fe80::1%interfaz).
#define HOST_SIZE (1025)

int
main(int argc, char **argv)
{
    struct addrinfo hints, *result, *rp;
    struct sockaddr_in6 *addr6;
    char host[HOST_SIZE];
    char plain[INET6_ADDRSTRLEN];

    //LAB A2: Acepta un nombre de host como argumento
    //LAB    argc = 2: argv[0] es el programa y argv[1] el nombre de host.
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
        return EXIT_FAILURE;
    }

    memset(&hints, 0, sizeof(hints));

    //EXP: AF_UNSPEC pide direcciones IPv4 e IPv6.
    //LAB A2: Todas las direcciones: IPv4 e IPv6
    //LAB    ai_family: familia de direcciones que se busca.
    //LAB    AF_UNSPEC: cualquiera; AF_INET daría solo IPv4 y AF_INET6 solo IPv6.
    hints.ai_family = AF_UNSPEC;

    //EXP: Sin ai_socktype cada direccion apareceria varias veces (TCP, UDP, ...).
    hints.ai_socktype = SOCK_STREAM;

    //LAB A2: Use getaddrinfo(): de nombre a direcciones IP
    //LAB    argv[1]: nombre de host que se resuelve.
    //LAB    NULL: sin servicio, porque no interesa ningún puerto.
    //LAB    &hints: filtros (IPv4 e IPv6, y SOCK_STREAM para no repetir cada dirección).
    //LAB    &result: aquí deja el comienzo de una lista enlazada con los resultados.
    Getaddrinfo(argv[1], NULL, &hints, &result);

    //EXP: getaddrinfo() devuelve una lista enlazada; la recorremos completa.
    //LAB A2: Todas las direcciones asociadas al host
    //LAB    result: primer elemento; rp->ai_next: el siguiente; NULL: fin de la lista.
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET && rp->ai_family != AF_INET6) {
            continue;
        }

        //EXP: NI_NUMERICHOST: queremos la direccion IP, no el nombre del host.
        //LAB A2: Use getnameinfo(): de dirección binaria a texto
        //LAB    rp->ai_addr, rp->ai_addrlen: dirección completa (sockaddr) y su tamaño.
        //LAB    host, sizeof(host): dónde escribir el texto y su tamaño.
        //LAB    NULL, 0: no pedimos el nombre del servicio.
        //LAB    NI_NUMERICHOST: queremos la IP numérica, no el nombre del host.
        //LAB    Como recibe todo el sockaddr_in6, conoce sin6_scope_id y muestra la interfaz: fe80::1%lo0.
        Getnameinfo(rp->ai_addr, rp->ai_addrlen,
                    host, sizeof(host), NULL, 0, NI_NUMERICHOST);

        //LAB A2: Muestra cada dirección IPv4 e IPv6
        //LAB    Primer %s: la familia, elegida con ?: según ai_family; segundo %s: la IP en texto.
        printf("%s %s\n", rp->ai_family == AF_INET ? "IPv4" : "IPv6", host);

        //EXP: Solo en link-local comparamos con inet_ntop(): recibe solo los 16 bytes
        //EXP: de la direccion, no sin6_scope_id, y por eso no puede mostrar la interfaz.
        if (rp->ai_family == AF_INET6) {
            addr6 = (struct sockaddr_in6 *)rp->ai_addr;

            if (IN6_IS_ADDR_LINKLOCAL(&addr6->sin6_addr) &&
                //LAB A2: Diferencia entre inet_ntop() y getnameinfo() con link-local
                //LAB    AF_INET6: familia; &addr6->sin6_addr: solo los 16 bytes de la dirección.
                //LAB    plain, sizeof(plain): dónde escribir el texto y su tamaño; NULL indicaría un error.
                //LAB    No recibe sin6_scope_id, por eso muestra fe80::1 sin la interfaz.
                inet_ntop(AF_INET6, &addr6->sin6_addr, plain, sizeof(plain)) != NULL) {
                printf("     inet_ntop(): %s (scope id %lu)\n",
                       plain, (unsigned long)addr6->sin6_scope_id);
            }
        }
    }

    //EXP: Libera la lista reservada por getaddrinfo().
    freeaddrinfo(result);

    return EXIT_SUCCESS;
}
