#ifndef UNIX_H
#define UNIX_H

#include <sys/types.h>

int unix_server_new(const char *path);
int unix_client_new(const char *path);

int unix_socket_accept(const int server);
int unix_socket_read(const int usock, unsigned char *buffer, const size_t buffer_size);
int unix_socket_write(const int usock, const unsigned char *data, const size_t data_len);
int unix_socket_close(const int socket);

#endif /* UNIX_H */
