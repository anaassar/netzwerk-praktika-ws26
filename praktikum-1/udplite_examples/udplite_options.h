#ifndef UDPLITE_OPTIONS_H
#define UDPLITE_OPTIONS_H

#include <sys/types.h>
#include <netinet/in.h>

/* FreeBSD defines the UDP-Lite options in <netinet/udplite.h>
 * (UDPLITE_SEND_CSCOV = 2, UDPLITE_RECV_CSCOV = 4).
 * The fallback values below (10/11) are the Linux ones. */
#ifdef __FreeBSD__
#include <netinet/udplite.h>
#endif

#ifndef UDPLITE_SEND_CSCOV
#define UDPLITE_SEND_CSCOV 10
#endif

#ifndef UDPLITE_RECV_CSCOV
#define UDPLITE_RECV_CSCOV 11
#endif

#endif
