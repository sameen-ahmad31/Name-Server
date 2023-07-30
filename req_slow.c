#include <domain_sockets.h>
#include <ns_limits.h>
#include <poll_helpers.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <assert.h>
#include <poll.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int socket_desc;

	/* this program doesn't take any arguments */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <server_name> <sleep_time>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	socket_desc = domain_socket_client_create(argv[1]);
	if (socket_desc < 0) panic("req error: domain socket open -- cannot find domain socket file");

	sleep(atol(argv[2]));

	close(socket_desc);

	return 0;
}
