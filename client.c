/*
 * See README for details.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define	EXIT_USAGE	2

/* client configuration */
#define	MAXREMOTES	10	/* max allowed remote endpoints (on cmd line) */
#define	MAXITERS	35000	/* max conn attempts per endpoint */

/* client state */
int nremotes;			/* number of remotes created */

/*
 * Describes the parameters and state of each remote specified on the command
 * line.
 */
typedef struct {
	const char		*r_label;	/* "IP:port" (as input) */
	char			*r_inbuf;	/* modified copy of input */
	struct sockaddr_in	r_connaddr;	/* IP:port, parsed */
	int			r_nconns;	/* nr of successful conns */
} remote_t;

/* List of remotes. */
remote_t remotes[MAXREMOTES];

int
main(int argc, char *argv[])
{
	int i, j, which;
	char *colon, *endp;
	unsigned long port;
	remote_t *rp;
	int nfails = 0;

	if (argc == 1) {
		errx(EXIT_USAGE, "must provide at least one remote");
	}

	if (argc - 1 > MAXREMOTES) {
		errx(EXIT_USAGE, "can only specify %d remotes", MAXREMOTES);
	}

	for (i = 1; i < argc; i++) {
		which = nremotes++;

		/*
		 * Initialize the remote's structure.
		 */
		rp = &remotes[which];
		rp->r_label = argv[i];
		rp->r_inbuf = strdup(argv[i]);
		assert(rp->r_inbuf != NULL); // XXX
		rp->r_nconns = 0;

		/* Separate out the TCP port number. */
		colon = strtok(rp->r_inbuf, ":");
		assert(colon != NULL);
		assert(colon == rp->r_inbuf);
		colon = strtok(NULL, ":");
		if (colon == NULL) {
			errx(EXIT_USAGE, "remote is missing a port: \"%s\"",
			    argv[i]);
		}

		/* Parse and validate the TCP port number. */
		errno = 0;
		port = strtoul(colon, &endp, 10);
		if (port == 0 && errno != 0) {
			err(EXIT_USAGE, "bad port for remote: \"%s\"", argv[i]);
		}

		if (*endp != '\0') {
			err(EXIT_USAGE,
			    "bad port for remote (trailing garbage): \"%s\"",
			    argv[i]);
		}

		if (port == 0 || port >= UINT16_MAX) {
			errx(EXIT_USAGE,
			    "bad port for remote (out of range): \"%s\"",
			    argv[i]);
		}

		/* Parse out the IP address. */
		bzero(&rp->r_connaddr, sizeof (rp->r_connaddr));
		rp->r_connaddr.sin_family = AF_INET;
		rp->r_connaddr.sin_port = htons(port);

		if (inet_pton(AF_INET, rp->r_inbuf, &rp->r_connaddr.sin_addr) != 1) {
			errx(EXIT_USAGE,
			    "bad IP address for remote: \"%s\"", argv[i]);
		}

		/*
		 * At this point, we've populated the remote_t with a
		 * human-readable label, plus a structure we can pass to
		 * connect(3socket) as much as we want to establish connections.
		 */
		(void) printf("will connect to IP %s port %d\n",
		    inet_ntoa(rp->r_connaddr.sin_addr),
		    ntohs(rp->r_connaddr.sin_port));
	}

	/*
	 * For each iteration, establish one connection to each remote IP and
	 * port.  We do it this way rather than walking through each remote
	 * first so that we can better assess whether limits appear to be
	 * per-remote or global.
	 */
	for (i = 0; i < MAXITERS; i++) {
		for (j = 0; j < nremotes; j++) {
			int sockfd;

			rp = &remotes[j];
			sockfd = socket(PF_INET, SOCK_STREAM, 0);
			if (sockfd < 0) {
				err(EXIT_FAILURE,
				    "iteration %d: remote %d (\"%s\"): socket",
				    i, j, rp->r_label);
			}

			if (connect(sockfd, (struct sockaddr *)&rp->r_connaddr,
			    sizeof (rp->r_connaddr)) != 0) {
				warn("iteration %d: remote %d (\"%s\"): connect",
				    i, j, rp->r_label);
				nfails++;
			} else {
				rp->r_nconns++;
			}
		}

		if (nfails > 0) {
			break;
		}
	}

	(void) printf("terminating after %d iterations\n", i);
	(void) printf("reason: %s\n", i == MAXITERS ?
	    "reached max iterations" : "connect failure");
	(void) printf("current (final) connection counts for each remote:\n");
	for (i = 0; i < nremotes; i++) {
		rp = &remotes[i];
		(void) printf("    %5d %s\n", rp->r_nconns, rp->r_label);
	}
	return (0);
}
