/*
 * See README for details.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define	EXIT_USAGE	2

/* server configuration */
#define MAXPORTS 10	/* maximum ports allowed on command line */

/* current state */
int nports;				/* total ports specified */
int server_fds[MAXPORTS];		/* fd for each server socket */
pthread_t server_threads[MAXPORTS];	/* thread for each server socket */

static int bind_port(const char *);
static void *server_start(void *);

int
main(int argc, char *argv[])
{
	int i, which, newfd;
	void *unused;

	if (argc == 1) {
		errx(EXIT_USAGE, "must provide at least one port");
	}

	if (argc - 1 > MAXPORTS) {
		errx(EXIT_USAGE, "can only specify %d ports", MAXPORTS);
	}

	for (i = 1; i < argc; i++) {
		which = nports++;
		assert(which == i - 1);
		assert(which >= 0);
		assert(which < MAXPORTS);

		newfd = bind_port(argv[i]);
		if (newfd < 0) {
			errx(EXIT_FAILURE, "failed to bind to port \"%s\"",
			    argv[i]);
		}

		server_fds[which] = newfd;
		(void) printf("bound to port %s (fd %d)\n", argv[i], newfd);
		if (pthread_create(&server_threads[which], NULL, 
		    server_start, (void *)which) != 0) {
			err(EXIT_FAILURE, "failed to start thread");
		}
	}

	assert(nports > 0);
	for (i = 0; i < nports; i++) {
		if (pthread_join(server_threads[i], &unused) != 0) {
			err(EXIT_FAILURE, "pthread_join");
		}

		(void) close(server_fds[i]);
	}

	return (0);
}

static int
bind_port(const char *numstr)
{
	unsigned long port;
	char *next;
	int sockfd;
	struct sockaddr_in addr;

	errno = 0;
	port = strtoul(numstr, &next, 10);
	if (port == 0 && errno != 0) {
		warn("parsing \"%s\"", numstr);
		return (-1);
	}

	if (*next != '\0' || port == 0 || port >= UINT16_MAX) {
		warnx("parsing \"%s\": invalid TCP port", numstr);
		return (-1);
	}

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		warn("socket");
		return (-1);
	}

	bzero(&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
		warn("bind");
		(void) close(sockfd);
		return (-1);
	}

	if (listen(sockfd, 128) < 0) {
		warn("listen");
		(void) close(sockfd);
		return (-1);
	}

	return (sockfd);
}

static void *
server_start(void *whicharg)
{
	int which = (int)(uintptr_t)whicharg;
	int fd = server_fds[which];
	int clientfd;
	struct sockaddr_in client;
	size_t clientlen;

	assert(fd >= 0);
	assert(which >= 0);
	assert(which < nports);
	(void) printf("thread %d: fd %d\n", which, fd);

	for (;;) {
		/* We're ignoring all incoming connections. */
		clientlen = sizeof (client);
		clientfd = accept(fd, (struct sockaddr *)&client, &clientlen);
		if (clientfd < 0) {
			err(EXIT_FAILURE, "accept");
		}
	}

	return (NULL);
}
