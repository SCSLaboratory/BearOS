#pragma once

/* Structure */
struct pollfd {
	int fd;
	short events;
	short revents;
};

/* Definition */
int poll(struct pollfd *fds, int nfds, int timeout);

/* Events and revents */
#define POLLIN          0x0001
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLPRI         0x0002
#define POLLHUP         0x0010
#define POLLNVAL        0x0020
#define POLLRDNORM      0x0040
#define POLLNORM        POLLRDNORM
#define POLLWRNORM      POLLOUT
#define POLLRDBAND      0x0080
#define POLLWRBAND      0x0100
