This repo has a pair of test programs intended for testing system limits related
to TCP connection counts.

## Build

Build with:

    make

Clean up with:

    make clean

## Test programs

    server PORT...

The `server` program takes 1-10 TCP port numbers (e.g., "12345") as command-line
arguments and binds sockets to each one.  It spins up a thread for each socket
to accept connections in a loop.  It doesn't do anything with these connections.
The fds will stay open until the program exits.

    client IP:PORT...

The `client` program takes 1-10 IP/port pairs (e.g., "127.0.0.1:12345").  It
attempts to connect to each one in turn, then repeats this process.  So if you
have three remotes, it connects to remote 1, then remote 2, then remote 3, and
then it goes back to remote 1, then remote 2, and so on.  It never closes these
connections.  The loop continues until one of the connection attempts fails.  It
will continue the current iteration and then stop, reporting how many successful
attempts it made to each remote.

## Examples

On SmartOS systems (tested on `joyent_20160830T004914Z`), the default number of
ephemeral TCP ports is 32,768.  If you create a server on one port:

    $ ./server 20020
    bound to port 20020 (fd 3)
    thread 0: fd 3

and use the client to connect to that port, this completes 32,768 connections
before failing with `EADDRNOTAVAIL`:

    $ ./client 127.0.0.1:20020
    will connect to IP 127.0.0.1 port 20020
    client: iteration 32768: remote 0 ("127.0.0.1:20020"): connect: Cannot assign requested address
    terminating after 32768 iterations
    reason: connect failure
    current (final) connection counts for each remote:
        32768 127.0.0.1:20020

On a test system, this took about 24 seconds.

For more than two TCP ports, you need to bump the fd limit accordingly in the
shell sessions that you use to run _both_ the client and the server:

    prctl -n process.max-file-descriptor -v 131072 -r -i process $$
    ulimit -n 131072

Now, we can try again with three local ports:

    $ ./server 20030 20031 20032
    bound to port 20030 (fd 3)
    thread 0: fd 3
    bound to port 20031 (fd 4)
    bound to port 20032 (fd 6)
    thread 1: fd 4
    thread 2: fd 6

    ./client 127.0.0.1:20030 127.0.0.1:20031 127.0.0.1:20032
    will connect to IP 127.0.0.1 port 20030
    will connect to IP 127.0.0.1 port 20031
    will connect to IP 127.0.0.1 port 20032
    client: iteration 32768: remote 0 ("127.0.0.1:20030"): connect: Cannot assign requested address
    client: iteration 32768: remote 1 ("127.0.0.1:20031"): connect: Cannot assign requested address
    client: iteration 32768: remote 2 ("127.0.0.1:20032"): connect: Cannot assign requested address
    terminating after 32768 iterations
    reason: connect failure
    current (final) connection counts for each remote:
        32768 127.0.0.1:20030
        32768 127.0.0.1:20031
        32768 127.0.0.1:20032

This test took 1m17s on a test machine.
