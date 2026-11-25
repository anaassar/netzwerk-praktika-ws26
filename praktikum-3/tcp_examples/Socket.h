
#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>

/*
 * EXP: Funciones wrapper de la API de sockets (igual que en udp_examples).
 *
 * - Accept(), Read(), Recv() y Send() repiten la llamada si fue
 *   interrumpida por una senal (errno == EINTR). Select() devuelve 0 con
 *   los conjuntos vacios.
 * - Los errores de preparacion (socket, bind, listen, connect, ...) son
 *   fatales: perror() y exit(EXIT_FAILURE).
 * - Recv(), Send() y Send_all() devuelven -1 ante errores que solo
 *   afectan a una conexion (ECONNRESET, EPIPE, ETIMEDOUT). Asi un servidor
 *   puede cerrar ese cliente y seguir funcionando. Otros errores son fatales.
 * - Accept() devuelve -1 si el cliente aborto la conexion antes de
 *   accept() (ECONNABORTED) o si faltan recursos (EMFILE, ENFILE, ENOBUFS,
 *   ENOMEM; en este caso tras una pausa de 50 ms).
 * - Shutdown() devuelve -1 si la conexion ya no existe (ENOTCONN, ...).
 * - Close() devuelve -1 ante EINTR o un error de conexion (FreeBSD:
 *   ECONNRESET). El descriptor ya esta cerrado: no repetir la llamada.
 * - Getaddrinfo() y Getnameinfo() no usan errno, sino codigos EAI_*
 *   (gai_strerror()). Sus errores son fatales.
 */

int Socket(int domain, int type, int protocol);

int Setsockopt(int sockfd, int level, int optname,
        const void *optval, socklen_t optlen);

int Bind(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen);

int Listen(int sockfd, int backlog);

int Accept(int sockfd,
        struct sockaddr *addr,
        socklen_t *addrlen);

int Connect(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen);

int Select(int nfds,
        fd_set *readfds,
        fd_set *writefds,
        fd_set *exceptfds,
        struct timeval *timeout);

ssize_t Read(int fd, void *buf, size_t len);

ssize_t Recv(int sockfd, void *buf,
        size_t len, int flags);

ssize_t Send(int sockfd, const void *buf,
        size_t len, int flags);

ssize_t Send_all(int sockfd, const void *buf, size_t len);

int Shutdown(int sockfd, int how);

int Close(int fd);

/* EXP: Convierte un texto en un puerto (1 a 65535). Si no es valido, termina. */
unsigned short Parse_port(const char *text);

int Getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints,
        struct addrinfo **res);

int Getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
        char *host, socklen_t hostlen,
        char *serv, socklen_t servlen,
        int flags);

#endif
