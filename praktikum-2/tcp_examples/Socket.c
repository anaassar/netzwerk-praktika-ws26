#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
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

    //LAB A4: Funciona también si send() acepta solo una parte de los datos
    //LAB    len: bytes que hay que enviar; total: bytes que el kernel ya aceptó.
    //LAB    send() puede aceptar menos de lo pedido, así que se repite hasta que no falte nada.
    while (total < len)
    {
        //LAB A4: Envía solo la parte que falta
        //LAB    p + total: empieza después de lo ya enviado.
        //LAB    len - total: cuántos bytes faltan; 0: sin opciones especiales.
        //LAB    sent: cuántos aceptó el kernel esta vez. Ejemplo: de 1000 acepta 400; después se envían 600 desde p + 400.
        ssize_t sent = Send(sockfd, p + total, len - total, 0);

        if (sent < 0)
        {
            return -1;
        }

        //LAB A4: Cuenta los bytes que el kernel aceptó
        //LAB    Sin esta suma se volvería a enviar lo mismo. La conversión es segura: sent < 0 ya se descartó.
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
