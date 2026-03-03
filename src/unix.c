#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

struct unix_socket {
	int fd;
	struct sockaddr_un address;
};

int unix_client_new(const char *path)
{
	struct sockaddr_un address;
	int err;
	int fd;

	err = 0;

	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, sizeof(address.sun_path), "%s", path);

	if ((fd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
		err = -errno;
		goto cleanup;
	}

	if (connect(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		err = -errno;
		goto cleanup;
	}

cleanup:
	if (!err) {
		err = fd;
	} else if (fd >= 0) {
		close(fd);
	}

	return err;
}

int unix_server_new(const char *path)
{
	struct sockaddr_un address;
	int err;
	int fd;

	err = 0;

	if ((fd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
		err = -errno;
		goto cleanup;
	}

	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, sizeof(address.sun_path), "%s", path);

	if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		err = -errno;
		goto cleanup;
	}

	if (listen(fd, 8) < 0) {
		return -errno;
		goto cleanup;
	}

cleanup:
	if (!err) {
		err = fd;
	} else if (fd >= 0) {
		close(fd);
	}

	return err;
}

int unix_socket_accept(const int server)
{
	int sock;

	if ((sock = accept(server, NULL, NULL)) < 0) {
		sock = -errno;
	}

	return sock;
}

int unix_socket_read(const int usock, unsigned char *buffer, const size_t buffer_size)
{
	int err;

	if ((err = read(usock, buffer, buffer_size)) < 0) {
		err = -errno;
	}

	return err;
}

int unix_socket_write(const int usock, const unsigned char *data, const size_t data_len)
{
	int err;

	if ((err = write(usock, data, data_len)) < 0) {
		err = -errno;
	}

	return err;
}

int unix_socket_close(const int usock)
{
	return close(usock) < 0 ? -errno : 0;
}
