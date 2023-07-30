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

/* Whatever information you need to save for a server (name, pipes, pid, etc...) */
struct server_info {
    struct pollfd poll;
	char * servernombre;
    int servpid;
    int read_pipe[2];
    int write_pipe[2];
    int sockdesc;
    char * bin;
};

/* All of our servers */
struct server_info servers[MAX_SRV];
int numero_server = 0;

/* Each file descriptor can be used to lookup the server it is associated with here */
struct server_info *client_fds[MAX_FDS];

/* The array of pollfds we want events for; passed to poll */
struct pollfd poll_fds[MAX_FDS];
int num_fds = 0;

/*
 * If you want to use these functions to add and remove descriptors
 * from the `poll_fds`, feel free! If you'd prefer to use the logic
 * from the lab, feel free!
 */
void
poll_create_fd(int fd)
{
	assert(poll_fds[num_fds].fd == 0);
	poll_fds[num_fds] = (struct pollfd) {
		.fd     = fd,
		.events = POLLIN
	};
	num_fds++;
}

void
poll_remove_fd(int fd)
{
	int i;
	struct pollfd *pfd = NULL;

	assert(fd != 0);
	for (i = 0; i < MAX_FDS; i++) {
		if (fd == poll_fds[i].fd) {
			pfd = &poll_fds[i];
			break;
		}
	}
	assert(pfd != NULL);

	close(fd);
	/* replace the fd by coping in the last one to fill the gap */
	*pfd = poll_fds[num_fds - 1];
	poll_fds[num_fds - 1].fd = 0;
	num_fds--;
}


int server_create(char *name, char *binary)
{
    // int ret;
    // int i;
    // int sv[2];
    // int socket_fd;
    // pid_t pid;

    if(name == NULL || binary == NULL)
    {
        panic("NULL!!");
    }


    struct server_info *server = &servers[numero_server];

    server->bin = binary;
    server->servernombre = name;

    if(pipe(server->read_pipe) < 0)
    {
        panic("PIPE ERROR");
    }

    if(pipe(server->write_pipe) < 0)
    {
        panic("PIPE ERROR");
    }

   /* Set up read and write pipes */
    // ret = pipe(sv);
    // if (ret < 0) {
    //     perror("pipe");
    //     return -1;
    // }

    server->servpid = fork();

    if (server->servpid < 0)
    {
        panic("FORK ERROR");
    }

    if(server->servpid == 0)
    {
        char * args[] = {binary, NULL};
        if(dup2(server->read_pipe[0], STDIN_FILENO) < 0)
        {
            panic("DUP2 ERROR");
        }
        if(dup2(server->write_pipe[1], STDOUT_FILENO) < 0)
        {
            panic("DUP2 ERROR");
        }
        close(server->read_pipe[0]);
		close(server->read_pipe[1]);
		close(server->write_pipe[0]);
		close(server->write_pipe[1]);
        execvp(args[0], args);
    }
    close(server->write_pipe[1]);
	close(server->read_pipe[0]);
    server->sockdesc = domain_socket_server_create(name);

    if(server->sockdesc < 0)
    {
        unlink(name);
        server->sockdesc = domain_socket_server_create(name);
        if(server->sockdesc < 0)
        {
            panic("SOCKET ERROR");
        }
    }
    poll_create_fd(server->sockdesc);
    numero_server ++;

 

    // /* Fork the server process */
    // pid = fork();
    // if (pid < 0) {
    //     perror("fork");
    //     return -1;
    // } else if (pid == 0) {
    //     /* Child process */
    //     close(sv[1]);
    //     dup2(sv[0], STDIN_FILENO);
    //     dup2(sv[0], STDOUT_FILENO);
    //     execl(binary, binary, (char*) NULL);
    //     perror("execl");
    //     _exit(1);
    // } else {
    //     /* Parent process */
    //     close(sv[0]);

    //     /* Allocate a new server_info struct */
    //     server = malloc(sizeof(struct server_info));
    //     if (!server) {
    //         perror("malloc");
    //         return -1;
    //     }

    //     /* Set up server_info struct */
    //     strncpy(server->name, name, MAX_NAME_LEN);
    //     server->pid = pid;
    //     server->read_pipe[0] = sv[0];
    //     server->write_pipe[1] = sv[1];

    //     /* Create a domain socket for the server */
    //     socket_fd = create_domain_socket(name);
    //     if (socket_fd < 0) {
    //         return -1;
    //     }
    //     server->socket_fd = socket_fd;

    //     /* Add the socket descriptor to poll_fds */
    //     poll_fds[num_fds].fd = socket_fd;
    //     poll_fds[num_fds].events = POLLIN;
    //     client_fds[socket_fd] = server;
    //     num_fds++;
    // }

    return 0;
}

int
main(int argc, char *argv[])
{
	ignore_sigpipe();

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <name_server_map_file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (read_server_map(argv[1], server_create)) panic("Configuration file error");

	/*** The event loop ***/
	while (1) 
    {
		int ret;
        struct server_info* server;
        char buffer[1024];
        char buff1[1024];
        int nuevo_client;
        int i;

		/*** Poll; wait for client/server activity, no timeout ***/
		ret = poll(poll_fds, num_fds, -1);
		if (ret <= 0) panic("poll error");

		/*** Accept file descriptors for different servers have new clients connecting! ***/
		/* ... */
        
        for(i = 0; i < numero_server; i++)
        {
            server = &servers[i];
            if(poll_fds[i].revents == POLLIN)
            {
                if((nuevo_client = accept(server->sockdesc, NULL, NULL)) == -1)
                {
                    panic("ACCEPT ERROR");
                }
                poll_create_fd(nuevo_client);
                client_fds[nuevo_client] = server;
                poll_fds[num_fds].revents = 0;
            }
        }


        for(i = numero_server; i < num_fds; i++)
        {
            server = client_fds[poll_fds[i].fd];
            if(poll_fds[i].revents == POLLIN)
            {
                int madrid; 
                poll_fds[i].revents = 0;
                memset(buffer, 0, 1024);
                memset(buff1, 0, 1024);
                madrid = read(poll_fds[i].fd, buffer, 1024);
                if(madrid == -1)
                {
                    if(errno == EPIPE)
                    {
                        continue;
                    }
                    panic("READ ERROR");
                }
                buffer[madrid] = '\0';
                if(madrid == 0)
                {
                    panic("ZERO READ ERROR");
                }
                write(server->read_pipe[1], buffer, strlen(buffer) + 1);
                madrid = read(server->write_pipe[0], buff1, 1024);
                if(madrid == -1)
                {
                    panic("READ ERROR");
                }
                buff1[madrid] = '\0';
                write(poll_fds[i].fd, buff1, madrid);
            }
            if(poll_fds[i].revents & (POLLHUP | POLLERR))
            {
                poll_fds[i].revents = 0;
                poll_remove_fd(poll_fds[i].fd);
                i--;
                continue;
            }
        }
    }





// 		for (int i = 0; i < num_fds && i < MAX_SRV; i++) {
//         if (poll_fds[i].revents & POLLIN) {
//             int server_fd = poll_fds[i].fd;

//             // Accept the client and get a new client descriptor
//             struct sockaddr_un client_addr;
//             socklen_t client_len = sizeof(client_addr);
//             int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
//             if (client_fd < 0) {
//                 perror("accept error");
//             } else {
//                 // Add the client to poll_fds
//                 poll_fds[num_fds].fd = client_fd;
//                 poll_fds[num_fds].events = POLLIN;
//                 client_fds[client_fd] = &servers[i];
//                 num_fds++;
//             }

//             // Reset revents for that entry
//             poll_fds[i].revents = 0;
//         }
//     }

//     // Communicate with existing clients
//     for (int i = MAX_SRV; i < num_fds; i++) {
//         if (poll_fds[i].revents & POLLIN) {
//             int client_fd = poll_fds[i].fd;
//             struct server_info *server = client_fds[client_fd];

//             // Read the message from the client fd
//             char buf[BUFSIZ];
//             ssize_t nread = read(client_fd, buf, BUFSIZ);
//             if (nread < 0) {
//                 perror("read error");
//             } else if (nread == 0) {
//                 // Client has hung up
//                 poll_remove_fd(client_fd);
//                 client_fds[client_fd] = NULL;
//             } else {
//                 // Write the message to the server write pipe
//                 ssize_t nwritten = write(server->write_pipe, buf, nread);
//                 if (nwritten < 0) {
//                     perror("write error");
//                 } else {
//                     // Read the reply from the server read pipe
//                     nread = read(server->read_pipe, buf, BUFSIZ);
//                     if (nread < 0) {
//                         perror("read error");
//                     } else {
//                         // Write the reply to the client fd
//                         nwritten = write(client_fd, buf, nread);
//                         if (nwritten < 0) {
//                             perror("write error");
//                         }
//                     }
//                 }
//             }

//             // Reset revents for that entry
//             poll_fds[i].revents = 0;
//         } else if (poll_fds[i].revents & POLLHUP) {
//             int client_fd = poll_fds[i].fd;

//             // Remove the client fd from poll_fds
//             poll_remove_fd(client_fd);
//             client_fds[client_fd] = NULL;

//             // Reset revents for that entry
//             poll_fds[i].revents = 0;
//         }
    


 		/*** Communicate with clients! ***/
		/* ... */
// 	}

	
// }

    return 0;
}
