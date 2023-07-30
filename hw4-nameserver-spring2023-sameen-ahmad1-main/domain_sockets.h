#pragma once

/*
 * Graciously taken from
 * https://github.com/troydhanson/network/tree/master/unixdomain
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

int
domain_socket_client_create(const char *file_name)
{
	struct sockaddr_un addr;
	int fd;

	/* Create the socket descriptor.  */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, file_name, sizeof(addr.sun_path) - 1);

	/* Attempt to connect the socket descriptor with a socket file named `file_name`. */
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) return -1;

	return fd;
}

int
domain_socket_server_create(const char *file_name)
{
	struct sockaddr_un addr;
	int fd;

	/* Create the socket descriptor.  */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, file_name, sizeof(addr.sun_path) - 1);

	/* Associate the socket descriptor with a socket file named `file_name`. */
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) return -1;
	/* How many clients can the system queue up before they are `accept`ed? */
	if (listen(fd, 5) == -1) return -1;

	return fd;
}
