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
	size_t s = 0;
	char *line = NULL;
	int socket_desc, exit_early = 0;

	/* this program doesn't take any arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	socket_desc = domain_socket_client_create(argv[1]);
	if (socket_desc < 0) panic("req error: domain socket open -- cannot find domain socket file");

	/* For each stdin input line, send a query to the server */
	while (getline(&line, &s, stdin) != -1) {
		int amnt, tot = 0;
		int len = strlen(line);
		/*
		 * Buffer to store the server response; remember, this is initialized
		 * to zeros (i.e. \0s)
		 */
		char resp[MAX_MSG_SZ + 1];

		/* Write the line to the server */
		while (tot < len) {
			amnt = write(socket_desc, line + tot, len - tot);
			if (amnt == 0) {
				exit_early = 1; /* domain socket is closed by server */
				break;
			}
			if (amnt == -1) panic("req error: writing to server in client");
			tot += amnt;
		}

		amnt = read(socket_desc, resp, MAX_MSG_SZ);
		if (amnt == -1) panic("req error: reading from server socket");
		/* make sure the string is \0-terminated */
		resp[amnt] = '\0';
		/* output the server's reply to the standard output */
		if (write(STDOUT_FILENO, resp, amnt) == -1) panic("req err: cannot write server reply to stdout");
	}
	if (!exit_early && !feof(stdin)) panic("req error: getline read from stdin");
	if (exit_early) panic("req error: Could not write entire message to server");

	/* clean up! */
	free(line);
	close(socket_desc);

	return 0;
}
