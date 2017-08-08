#include <poll.h>
#include <string.h>
#include <msg.h>
#include <sys/socket.h>
#include <netdb.h>	
#include <sbin/netd.h>

int poll(struct pollfd *fds, int nfds, int timeout) {
	Net_poll_t req;
	Msg_status_t status;
	int i;

	/* Zero REVENTS. */
	for(i = 0; i < nfds; i++) {
		fds[i].revents = 0;
	}

	/* Set up the message. */
	memset(&req, 0, sizeof(Net_poll_t));
	req.type = NET_POLL;
	memcpy(req.fds, fds, sizeof(struct pollfd) * nfds);
	req.numfds = nfds;
	req.timeout = timeout;
	msgsend(NETD, &req, sizeof(Net_poll_t));

	/* Use msgrecv to block until the poll ends. */
	msgrecv(NETD, &req, sizeof(Net_poll_t), &status);
	memcpy(fds, req.fds, sizeof(struct pollfd) * nfds);
	return req.rval;
}
