bin_PROGRAMS           = client daytime_server discard_server echo_server
client_SOURCES         = client.c Socket.c Socket.h
daytime_server_SOURCES = daytime_server.c Socket.c Socket.h
discard_server_SOURCES = discard_server.c Socket.c Socket.h
echo_server_SOURCES    = echo_server.c Socket.c Socket.h
echo_server_CFLAGS     = $(AM_CFLAGS) -pthread
echo_server_LDFLAGS    = -pthread
