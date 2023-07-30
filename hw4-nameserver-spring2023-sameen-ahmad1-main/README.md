# HW4: Nameserver

This homework will create a *nameserver* whose job is to enable *clients* to communicate with different *servers*/*services*.
It is called a *nameserver* as each service is associated with a different *name*, which is a unique string.
A client requests communication with the service, identifying it by *name*, and the nameserver's job is to connect it to the corresponding service.
For example, if I wanted to determine the wifi strength on my system, I'd want to talk to the service identified with the name "network_monitor".
You can think of the nameserver as providing a similar function to a browser (more strictly, to DNS): you enter a human-readable address in your address bar, and the browser (through DNS servers) converts that address into a numeric (IP) address identifying the server.

There are three classes of software involved in this:

1. **Clients** - For the homework, you'll simply use `req` (built using `req.c`) as a representative client.
    You shouldn't need to write your own.
	`req` is meant to fit into a pipeline: it sends whatever it reads from its standard input to the server, and then writes whatever it receives from the server into its own standard output.
2. **Servers** - Each server is associated with a terse string, a *name*.
    Servers are simple.
	They read a client request from their standard input, and write a reply to their standard output.
	We've included servers in `srv/`, and you can use any program that uses standard in and out for its I/O (e.g. `/bin/cat`).
	You shouldn't need to change servers.
3. The **nameserver** - The key component you'll focus on in this assignment.
	It receives client requests for a specific server identified by its name.
	It will route these requests to the corresponding server, then receive the server reply, and route it back to the client.

Communication between the nameserver and servers uses `pipe`s.
Communication between the nameserver and the clients uses domain sockets.
Domain sockets are used to enable per-client channels (using `accept` to create new channels).
To enable concurrent clients, the nameserver must use `poll`.

You've already written a simple version of the nameserver that assumes a single *server* in lab.
To generalize this to multiple servers, you'll create a named domain socket per server, with the name of the domain socket (in the file system) being the *name* associated with the server.
For example, an `echo` server's domain socket could be created with `domain_socket_server_create("echo")`.
Thus, the client specifies which server it wants to communicate with by connecting to the corresponding domain socket in the file system (you can see this in `req.c`).

The nameserver learns the names and servers it should support by taking a [csv file](https://en.wikipedia.org/wiki/Comma-separated_values) as a command line argument that includes `name,server_binary` pairs.
See examples in `server_map1.csv` and `server_map.csv`.
Note that we provide the code for parsing and using these files in `read_server_map(file, create_server_fn)`, so you should be able to simply use that.

**Summary:**
This assignment is to essentially take the code from lab that uses domain sockets and `poll` to communicate concurrently with multiple clients, and extend it to add multiple servers (instead of only one).
You should not start this from scratch, and should start with the lab code handy.

![An overview of the nameserver (grey box). Clients (pink processes) connect to the separate domains sockets (`./echo` and `./yell` here), and we create a per-client domain socket connection with which we communicate with them. The services (green processes) communicate with the nameserver using pipes (one per direction for `stdin` and `stdout`). The nameserver uses `poll` to detect and handle events on each of the domain sockets. For clients that have connected to a service's name, their requests are routed by the nameserver to the corresponding service, and its reply is send back to them.](overview.pdf)

## Example Nameserver Execution

Lets imagine that you've implemented the whole nameserver!
We can start up the nameserver with a single server (specified in `server_map1.csv`), then use a client to use its server.

```
$ ./ns server_map1.csv & sleep 1
$ echo "hello world" | ./req echo
hello world
```

The `sleep 1` is there to ensure that the `ns` executes and creates the corresponding `echo` domain server before running the client.
The client asks to use the `echo` server, and passes it `hello world`.
The server echos it right back, and `req` outputs it.

Note that at this point the `ns` is *still executing*.
Lets use our shell skills we got from HW3 to help us out here to stop it:

```
$ jobs
[1]+  Running                 ./ns server_map1.csv &
$ fg 1
<cntl-c>
$ jobs
```

After foregrounding the nameserver, we use control-c to terminate it, thus the second `jobs` command shows no background commands.
Please remember, you're using a shared server, and need to kill your processes.

## Nameserver Design

When the nameserver starts executing, it will create each server, including the pipes to communicate with it, and a domain socket with a filesystem name corresponding to the server's *name*^[This differs from the server provided with the lab in which the socket name was provided on the command line. Now each server will have a different domain socket!].
After the servers have been created, the nameserver will go into its event loop.
Similar to the code from lab, the nameserver will be focused around an event loop that

- will detect new clients trying to connect to the domain socket, and `accept` them,
- will handle the requests for clients that have previously connected by routing them to the corresponding server, and
- will detect any closed connections to clean up the data-structures.

When you get an event, you'll require a means to differentiate between an event on a descriptor that requires you to `accept` a new client, and a normal request from a client.
In the lab, the entry at index `0` in the `pollfd` array is always the socket to create new client connections.
We suggest that you generalize that in this assignment by treating the lowest *N* entries in the `pollfd` array as server domain sockets that you'll use to `accept` new clients.
Higher entries (>= *N*) are client descriptors which will carry client requests.

To handle client requests, you must determine to which server they are making a request.
Thus, we suggest that you track, for each descriptor, the server it is associated with.
A straight-forward implementation of this is an array of pointers, indexed by the file descriptor, that references the server (structure) associated with the descriptor.

## Nameserver Data-structures

The `ns.c` file includes some prototype data-structures.
You can use these should you want to.
These recommended data-structures for this assignment are:

- An array of `struct pollfd`s (`poll_fds`).
    This is similar to that used in your labs, and is passed to `poll`.
	It will include all of the your file descriptors for

	1. each client's domain socket, and
	2. each server's domain socket used to `accept` new client connections.

	Note that you can assume that the servers reply promptly, so we don't need to `poll` on the pipe descriptors.

	In the example lab code that provides a *single server*, we hard-code that the `0`th offset is the server's domain socket descriptor we use to `accept` new clients.
	In this homework, you'll need to answer the question of how to determine if the `pollfd` that `poll` says has an event requires `accept`ing a new client, or is a channel already talking to a specific client.
	I'd suggest that you can generalize the lab's code by ensuring that if you have `N` servers, the lowest `N` entries in the `pollfd` array contain the descriptors to use to connect to servers.

- An array of `struct server_info` structures storing data for each server (`servers`).
    This might include

	1. the server's name,
	2. the server's binary file,
	3. the server's domain socket descriptor on which we `accept` new clients,
	4. the server's `pipe` descriptors we can use to communicate with it, and
	5. the server's pid.

	Each server will have an entry in this array.

- Finally, when `poll` returns that there was an event on a descriptor, we need to know if it is a new client, and we need to `accept` on the descriptor, or if it is a client request we need to pass to a specific server.
    To track this, we can use an array of pointers (e.g. `client_fds`) to the server structure above, indexed by the descriptor.
    When `poll` says that, for example, descriptor `42` has an event, we can look up the 42nd entry the array to find which server that request corresponds to.

## Assumptions

- You can focus on `POLLIN` events (and, of course `POLLHUP | POLLERR`), and ignore `POLLOUT` events.
    In other words, you can assume that your `write`s don't block (similar to the lab code).
- You can assume constant limits on many aspects of the system, detailed in `ns_limits.h`.
- You can use `panic` in response to many errors (see examples in the template code).
    However, you should not use it when client actions could cause errors (e.g. when `read`ing/`write`ing from client's domain socket).

## Files and Resources

- `ns.c` is your nameserver implementation.
    The initial contents is full of utility functions and suggests; feel free to change or ignore at will.
- `srv/*.c` is your servers.

    - `srv/yell.c` upper-cases the input.
    - `srv/fault.c` triggers a fault in the server on the fifth request.

- `req.c` is your typical client that makes `req`uests to the nameserver's servers.
- `req_slow.c` opens a client connection to the nameserver, and keeps it open, but makes no requests for a given number of seconds.
    This is useful to test that your program is properly handling concurrency.
	It allows you to create multiple client connections (using, for example, `./req_slow echo 100 &`), then to use normal requests to make sure that it returns immediately (for example, with `./req echo`).
- `poll_helpers.h` is a file including many helper functions you may want to use.
    This includes:

	- `PRINT_HERE()` which will simply help you with "print debugging".
	    It prints the current file name, the current function, and the line of this print statement to the standard error.
	- `panic(msg)` prints any system call error that occured, the `msg`, and `exit`s the program.
    - `pollfds_print(pollfd_array, ...)` print out all of the file descriptors and their status in the `pollfd_array` returned from `poll`.
	    Useful for debugging.
    - `ignore_sigpipe()` will set the program to ignore any `SIGPIPE`s that occur because a client closes its domain socket.
	    It means that `read`s and `write`s to client domain sockets in the nameserver will return `0` or `-1` should the client close it or fail.
		The next time `poll` is called after such a domain socket experiences a failure, it will return that file descriptor's `revents & (POLLHUP | POLLERR)`.
	- `read_server_map(filename, server_create_fn)` opens the server map file, and calls the `server_create_fn` for each server specified in that csv file.

## Milestones

This assignment has a single required milestone, and an optional (extra credit) module.

### Milestone 0: Multi-Client, Multi-Server Nameserver

This milestone will focus on the main goal: a nameserver that supports multiple clients and multiple servers.

#### Tests

We won't provide automated tests for this homework.
You can get points for each of the following tests:

- **Single server, single client** - effectively just the lab's code.

	```
    $ ./ns server_map1.csv & sleep 1 ; echo "hello world" | ./req echo
	```

    This should output `hello world`.
- **Large output** - We want to be able to correctly process large sequences of requests; we'll use the contents of `README.md`!

	```
	$ ./ns server_map1.csv & sleep 1
	$ cat README.md | ./req echo > test.txt
	$ diff README.md test.txt
	```

    Here, `diff` compares its two arguments, and outputs if they are different.
	In this case, they should *not* be different, thus `diff` should output nothing.
- **Multiple clients** - Lets make sure that multiple client requests can be handled correctly; also mostly lab code.

    ```
	$ ./ns server_map1.csv & sleep 1
	$ echo "hello world" | ./req echo & echo "hello world" | ./req echo
	```

    This should output `hello world` twice, but because it is concurrent, it might output them interleaved.
- **Multiple clients, multiple servers** - client requests for different servers now.

    ```
	$ ./ns server_map.csv & sleep 1
	$ echo "hello world" | ./req echo & echo "hello world" | ./req yell
	```

    This should output `hello world` and `HELLO WORLD`, also potentially interleaved.
- **Same, but with large output** - again, lets use multiple client requests, but with one making many requests with large amounts of output.

    ```
	$ ./ns server_map.csv & sleep 1
	$ echo "hello world" | ./req yell & cat README.md | ./req echo > output.txt
	$ diff output.txt README.md
	```

    This should output only `HELLO WORLD`, as the `diff` should return no differences (similar to the `diff` results above).
- **Testing concurrency** - Multiple clients and servers, with many clients with open connections concurrently.

    ```
	$ ./ns server_map.csv & sleep 1
	$ ./req_slow echo 10 & ./req_slow yell 10 & ./req_slow echo 10 & ./req_slow yell 10 &
	$ ./req_slow echo 10 & echo "hello world" | ./req yell & echo "hello world" | ./req echo
	```

	This should output `HELLO WORLD` and `hello world` after a second, and *not* after the 10 seconds of all of the slow requests.

Note that all of the `sleep 1`s after we run `ns` are there to make sure that the nameserver starts up and sets up its domain sockets before executing the rest of the commands.
Remember that you can put a background command into the foreground using `fg`, and you can terminate a foreground task using cntl-c.
Concretely, after you run the examples above, a quick `fg` should make the `./ns` process foreground, and cntl-c should terminate it, allowing you to run it again.
To see if you're running servers you need to terminate, use `jobs`.

We will *not* test your implementation with `valgrind` for this assignment.

### Milestone 1: Server Reboot

This milestone is *extra credit*.
Not doing it will not negatively impact your grade at all.

Servers may fail.
If a server fails, you'll want to detect its failure (either through the return values for pipe `read`/`write`s, or by catching `SIGCHLD`), and restart it again.
You don't want to recreate its domain socket^[Doing so could negatively impact connected clients.], but you'll want to create new pipes, and `exec` a new process for the server.

#### Tests

The `srv/fault.bin` program (with name `flt`) faults out every 5 inputs.
For example, if your nameserver does not implement server reboots, it will behave as such:

```
$ ./ns server_map.csv & sleep 1
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt
# either the server faults here, or "hangs" without reply.
```

Note that the last request either causes the nameserver to fault, exit, or "hang" without providing a reply due to the server fault.
This milestone will add server rebooting so that it will behave more like:

```
$ ./ns server_map.csv & sleep 1
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt
HELLO WORLD
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
HELLO WORLD
HELLO WORLD
$ echo "hello world" | ./req flt
HELLO WORLD
```

This milestone's points are broken into two sets:

1. The nameserver closes the client domain socket when the server it is communicating with faults out.
    Thus the client request that causes a server fault will not receive a reply.
	An example output:

    ```
    $ ./ns server_map.csv & sleep 1
    $ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
    HELLO WORLD
    HELLO WORLD
	$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
    HELLO WORLD
    HELLO WORLD
    $ echo "hello world" | ./req flt
    $ echo "hello world" | ./req flt
    HELLO WORLD
    ```

	Notice how the fifth client didn't receive output!

2. The nameserver reboots the server *and* sends any pending client requests to it.
    Example output:

    ```
    $ ./ns server_map.csv & sleep 1
    $ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
    HELLO WORLD
    HELLO WORLD
	$ echo "hello world" | ./req flt ; echo "hello world" | ./req flt
    HELLO WORLD
    HELLO WORLD
    $ echo "hello world" | ./req flt
    HELLO WORLD
    $ echo "hello world" | ./req flt
    HELLO WORLD
    ```

We will *not* test your implementation with `valgrind` for this assignment.

## Where to Start

You already have the following code:

- Domain socket code from lecture.
- Domain socket code from lab for communication between a single client and server with the server running an external program.
- Code from lecture that combines domain sockets and `poll`.
- Code from lab that combines domain sockets and `poll` to communicate between multiple clients and a single server.

All of these use domain sockets for communication with the client, and pipes for communication with the server.
You want to start this homework from the code for the lab that combines domain sockets with `poll`.
Your main job is to extend it to work with multiple server programs, each with their own domain socket with communication organized by the *nameserver*.

Note that in the labs we often talked about the `server.c` communicating with the client.
This assignment is different, as the *servers* are what the *nameserver* creates, and the *nameserver* acts as the *middle-person* for communication between clients and those servers.

## Background

Why are we implementing a nameserver?
Believe it or not, this is a common, and important part of most modern systems.
For example, [`systemd` (link)](https://en.wikipedia.org/wiki/Systemd) is a key service in most modern Linux systems.
It is modeled after `launchd`, the corresponding service in OSX.
`systemd` acts as a *middle-person* for communication between client and system services (servers).

Clients use the [D-Bus (link)](https://en.wikipedia.org/wiki/D-Bus) protocol for communication over domain sockets with `systemd`.
D-Bus includes specifying which service a client wishes to communicate with, then a means to send requests, and receive responses from it.
Sound familiar!?
Note that Android includes a similar communication medium called [*Binder* (link)](https://en.wikipedia.org/wiki/OpenBinder).

`systemd` acts as this middle-person so that it can provide various functionalities in managing the services.
For example, `systemd`:

- Controls the boot-up of all system software including services (replacing a traditional `initrd` implementation).
- Tracks when services are actually needed, and only boot them up when a client requests their service.
    In this way, only services that are needed, take up system memory and processing time.
- Will reboot services that fail to improve the reliability of the system.
- Provides a complex specification of names that a services provides, and dependencies on other services it has.

You can view the services that `systemd` is running and their status with the command:

```
$ service --status-all
 [ + ]  acpid
 [ - ]  alsa-utils
 [ - ]  anacron
 [ + ]  apparmor
 [ + ]  apport
 [ + ]  avahi-daemon
 [ - ]  bluetooth
 [ - ]  console-setup.sh
 [ + ]  cron
 [ + ]  cups
 [ + ]  cups-browsed
 [ + ]  dbus
 [ + ]  gdm3
 [ - ]  grub-common
 [ - ]  hwclock.sh
 [ + ]  irqbalance
 [ + ]  kerneloops
 [ - ]  keyboard-setup.sh
 [ + ]  kmod
 [ + ]  network-manager
 [ + ]  openvpn
 [ - ]  plymouth
 [ - ]  plymouth-log
 [ + ]  postgresql
 [ - ]  pppd-dns
 [ + ]  procps
 [ - ]  pulseaudio-enable-autospawn
 [ - ]  rsync
 [ + ]  rsyslog
 [ - ]  saned
 [ - ]  speech-dispatcher
 [ - ]  spice-vdagent
 [ + ]  sysstat
 [ + ]  udev
 [ + ]  ufw
 [ + ]  unattended-upgrades
 [ - ]  uuidd
 [ + ]  whoopsie
 [ - ]  x11-common
```

You can see services for the graphical user interface (`x11-common`), network services (e.g. wifi management, `network-manager`), and vpn (`openvpn`).

The *nameserver* of this assignment is motivated by `systemd`.
We're vastly simplifying communication compared to D-Bus and instead making it mimic the streams of normal standard input and output; we're booting up our servers/services at the time that `ns` starts executing; and we're using a very simple implementation of the specification of names their corresponding services.
