#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "Socket.h"

/* EXP: Errores que solo afectan a una conexion (el otro extremo ya no esta). */
static int
connection_error(int err)
{
    return (err == ECONNRESET || err == EPIPE || err == ETIMEDOUT);
}

int Socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);

    if (fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int Setsockopt(int sockfd, int level, int optname,
        const void *optval, socklen_t optlen)
{
    if (setsockopt(sockfd, level, optname, optval, optlen) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int Bind(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen)
{
    if (bind(sockfd, addr, addrlen) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int Listen(int sockfd, int backlog)
{
    if (listen(sockfd, backlog) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/* EXP: Errores de accept() por falta temporal de recursos (demasiados
 * descriptores abiertos o falta de memoria). */
static int
resource_error(int err)
{
    return (err == EMFILE || err == ENFILE || err == ENOBUFS || err == ENOMEM);
}

int Accept(int sockfd,
        struct sockaddr *addr,
        socklen_t *addrlen)
{
    /* EXP: 50 ms */
    const struct timespec pause = {0, 50000000L};
    int fd, err;

    for (;;)
    {
        fd = accept(sockfd, addr, addrlen);

        if (fd >= 0)
        {
            return fd;
        }

        if (errno == EINTR)
        {
            continue;
        }

        /* EXP: El cliente aborto la conexion antes de que llamaramos a accept(). */
        if (errno == ECONNABORTED)
        {
            return -1;
        }

        err = errno;
        perror("accept");

        /* EXP: No es fatal: cuando se vaya algun cliente habra recursos otra vez.
         * La conexion sigue en la cola de listen(), asi que accept() fallaria
         * de inmediato una y otra vez; la pausa evita gastar toda la CPU. */
        if (resource_error(err))
        {
            nanosleep(&pause, NULL);
            return -1;
        }

        exit(EXIT_FAILURE);
    }
}

int Connect(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen)
{
    if (connect(sockfd, addr, addrlen) < 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int Select(int nfds,
        fd_set *readfds,
        fd_set *writefds,
        fd_set *exceptfds,
        struct timeval *timeout)
{
    int result = select(nfds, readfds, writefds, exceptfds, timeout);

    if (result < 0)
    {
        if (errno == EINTR)
        {
            /* EXP: Tras EINTR el contenido de los conjuntos es indefinido:
             * los vaciamos y devolvemos 0 ("nada listo"); el llamador los reconstruye. */
            if (readfds != NULL)
                FD_ZERO(readfds);
            if (writefds != NULL)
                FD_ZERO(writefds);
            if (exceptfds != NULL)
                FD_ZERO(exceptfds);
            return 0;
        }

        perror("select");
        exit(EXIT_FAILURE);
    }

    return result;
}

ssize_t Read(int fd, void *buf, size_t len)
{
    ssize_t result;

    do
    {
        result = read(fd, buf, len);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }

    return result;
}

ssize_t Recv(int sockfd, void *buf,
        size_t len, int flags)
{
    ssize_t result;

    do
    {
        result = recv(sockfd, buf, len, flags);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        int err = errno; /* EXP: guardamos errno: perror() puede modificarlo */

        perror("recv");

        if (connection_error(err))
        {
            return -1;
        }

        exit(EXIT_FAILURE);
    }

    return result;
}

ssize_t Send(int sockfd, const void *buf,
        size_t len, int flags)
{
    ssize_t result;

    do
    {
        result = send(sockfd, buf, len, flags);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        int err = errno; /* EXP: guardamos errno: perror() puede modificarlo */

        perror("send");

        if (connection_error(err))
        {
            return -1;
        }

        exit(EXIT_FAILURE);
    }

    return result;
}

/* EXP: send() puede aceptar menos bytes de los pedidos (envio parcial).
 * Send_all() llama a Send() hasta que se hayan enviado los len bytes. */
ssize_t Send_all(int sockfd, const void *buf, size_t len)
{
    const char *p = buf;
    size_t total = 0;

    while (total < len)
    {
        ssize_t sent = Send(sockfd, p + total, len - total, 0);

        if (sent < 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return (ssize_t)total;
}

int Shutdown(int sockfd, int how)
{
    if (shutdown(sockfd, how) < 0)
    {
        int err = errno;

        perror("shutdown");

        /* EXP: el otro extremo ya corto la conexion: no es fatal. */
        if (err == ENOTCONN || connection_error(err))
        {
            return -1;
        }

        exit(EXIT_FAILURE);
    }

    return 0;
}

int Close(int fd)
{
    if (close(fd) < 0)
    {
        int err = errno;

        perror("close");

        /* EXP: FreeBSD puede devolver ECONNRESET si el otro extremo corto la
         * conexion. El descriptor ya esta liberado: no es fatal y NO se debe
         * repetir close() (tampoco con EINTR), porque otro hilo podria haber
         * recibido el mismo numero de descriptor. */
        if (err == EINTR || connection_error(err))
        {
            return -1;
        }

        exit(EXIT_FAILURE);
    }

    return 0;
}

/* EXP: getaddrinfo() y getnameinfo() devuelven un codigo EAI_*;
 * solo con EAI_SYSTEM el error esta en errno. */
static void
gai_fatal(const char *name, int error)
{
    if (error == EAI_SYSTEM)
    {
        perror(name);
    }
    else
    {
        fprintf(stderr, "%s: %s\n", name, gai_strerror(error));
    }

    exit(EXIT_FAILURE);
}

int Getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints,
        struct addrinfo **res)
{
    //LAB A2, A3: Use getaddrinfo()
    //LAB    node: nombre de host o IP; service: servicio o puerto; hints: filtros; res: lista de resultados.
    //LAB    error: 0 si funcionó o un código EAI_* (no usa errno), que se traduce con gai_strerror().
    int error = getaddrinfo(node, service, hints, res);

    if (error != 0)
    {
        gai_fatal("getaddrinfo", error);
    }

    return 0;
}

int Getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
        char *host, socklen_t hostlen,
        char *serv, socklen_t servlen,
        int flags)
{
    //LAB A2: Use getnameinfo()
    //LAB    addr, addrlen: dirección que se convierte; host, hostlen: dónde escribir la IP o el nombre.
    //LAB    serv, servlen: dónde escribir el servicio; flags: opciones, por ejemplo NI_NUMERICHOST.
    //LAB    error: 0 si funcionó o un código EAI_* (no usa errno).
    int error = getnameinfo(addr, addrlen, host, hostlen,
                            serv, servlen, flags);

    if (error != 0)
    {
        gai_fatal("getnameinfo", error);
    }

    return 0;
}

/* EXP: atoi() no detecta errores: "abc" daria 0 y 70000 se truncaria a 4464. */
unsigned short Parse_port(const char *text)
{
    unsigned long port = 0;
    const char *p;

    for (p = text; *p >= '0' && *p <= '9' && port <= 65535; p++)
    {
        port = port * 10 + (unsigned long)(*p - '0');
    }

    /* EXP: *p != 0: caracter no valido o numero demasiado grande. Vacio y "0" dan port == 0. */
    if (*p != '\0' || port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", text);
        exit(EXIT_FAILURE);
    }

    return (unsigned short)port;
}
