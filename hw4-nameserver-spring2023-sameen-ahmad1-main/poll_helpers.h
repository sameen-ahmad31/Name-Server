#pragma once

#include <ns_limits.h>

#include <poll.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define NS_STRX(x) #x
#define NS_STR(x) NS_STRX(x)

/**
 * This is useful when you just want it to print out at a specific
 * location in a file. When you're doing print debugging to see where
 * execution is at, this is better than manual print outs. To use it,
 * just:
 *
 * ```c
 * PRINT_HERE();
 * ```
 *
 * It will print the file, function, and line in the function to
 * stderr. As always, make sure that your submitted program doesn't
 * have print outs outside of the specification.
 */
#define PRINT_HERE()							\
	do {								\
	fprintf(stderr, "%s%s%s\n", __FILE__ " @ ", __func__,  ":" NS_STR(__LINE__)); \
	fflush(stderr);							\
} while (0);

/**
 * If you get an error, you can call this to print out a message, and
 * exit from the program. If `errno` is set, it will print out with
 * `perror`, otherwise it will just print out the message. Either way,
 * the printouts go to `stderr`.
 */
static inline void
panic(char *msg)
{
	if (errno != 0) perror(msg);
	else fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

static inline void
pollfds_print(struct pollfd *fds, size_t nfds)
{
	size_t i;

	printf("Poll file descriptors\n");
	for (i = 0; i < nfds; i++) {
		short e = fds[i].revents;

		if (fds[i].fd == 0) continue;

		/* All of the file descriptors should be listening for input! */
		assert(fds[i].events & POLLIN);
		printf("\t fd %d with events: %s%s%s%s.\n", fds[i].fd,
		       e & POLLIN  ? " READ" : "",
		       e & POLLOUT ? " WRITE" : "",
		       e & (POLLERR | POLLHUP) ? " CLOSE" : "",
		       (e & (POLLERR | POLLHUP | POLLOUT | POLLIN)) == 0 ? " NO EVENTS" : "");
	}
}

/**
 * Call this in `main` before you do anything of interest so that when
 * a client disconnects, it won't cause the process to terminate on a
 * `SIGPIPE` signal. Instead any `read` or `write` will return `-1`
 * with `errno == EPIPE` so that you can handle the client's failure.
 */
static inline void
ignore_sigpipe(void)
{
	signal(SIGPIPE, SIG_IGN);
}

/**
 * Take the server `name`, and the `binary` program's name associated
 * with that server name. Return either `0` if you're able to create a
 * new process running the program, or `-1` if there is an error. The
 * function passed ownership of `name` and `binary`, thus must free
 * them.
 *
 * The `name` and `binary` will often be inserted into a global map of
 * the name and its association with the server (e.g. pipe fds for the
 * server).
 */
typedef int (*server_create_fn_t)(char *name, char *binary);

/**
 * Assumes that `filename` is a file that is a tab-delimited csv file
 * with two columns. The first, is the name to expose via the
 * nameserver, and the second column is the server's binary associated
 * with the name. The `fn` is called for each of this name, binary
 * pairs. The expectation is that the function will create the server
 * process, and will update any necessary global data-structures.
 */
static inline int
read_server_map(char *filename, server_create_fn_t fn)
{
	FILE *srv_map;
	char *lines[MAX_SRV] = {};
	int i, j;
	size_t sz = 0;

	srv_map = fopen(filename, "r");
	if (!srv_map) panic("Error opening the server configuration file");

	/* read in the whole file */
	for (i = 0; getline(&lines[i], &sz, srv_map) != -1; i++) {
		sz = 0;
	}
	/*
	 * Close the file's descriptors so that children processes
	 * don't see that descriptor.
	 */
	if (!feof(srv_map)) panic("Error: reading from the nameserver configuration file");
	fclose(srv_map);

	for (j = 0; j < i; j++) {
		char *name, *bin;

		name = strtok(lines[j], ",");
		bin  = strtok(NULL, "\n");
		if (!name || !bin) panic("Error: Formatting server configuration file");

		if (fn(strdup(name), strdup(bin))) panic("Error: creating server");
		free(lines[j]);
	}
	if (i == 0) panic("Error: nameserver configuration requires servers");

	return 0;
}
