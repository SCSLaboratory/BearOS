/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/
/*
 * libsocket.c
 *
 * Provides the library calls for BSD sockets.
 *
 * Author: Colin Nichols
 *
*/
#include <stdio.h>
#include <msg.h>
#include <syscall.h>		/* system call interface */
#include <string.h>		/* for memcpy */
#include <msg_types.h>

#include <sys/socket.h>		/* socket definitions */
#include <netdb.h>		/* hostent structure */

#include <sbin/netd.h>		/* network daemon interface */

int socket(int domain, int type, int protocol) {
	Net_socket_req_t req;
	Net_socket_resp_t resp;
	Msg_status_t status;

	req.type = NET_SOCKET;
	req.domain = domain;
	req.stype = type;
	req.protocol = protocol;

	msgsend(NETD, &req, sizeof(Net_socket_req_t));
	msgrecv(NETD, &resp, sizeof(Net_socket_resp_t), &status);
	return resp.sockfd;
}

int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen) {
	Net_bind_req_t req;
	Net_bind_resp_t resp;
	Msg_status_t status;

	req.type = NET_BIND;
	req.sockfd = sockfd;
	memcpy(&(req.my_addr), my_addr, sizeof(struct sockaddr));
	req.addrlen = addrlen;

	msgsend(NETD, &req, sizeof(Net_bind_req_t));
	msgrecv(NETD, &resp, sizeof(Net_bind_resp_t), &status);
	return resp.ret;
}

int listen(int sockfd, int backlog) {
	Net_listen_req_t req;
	Net_listen_resp_t resp;
	Msg_status_t status;

	req.type = NET_LISTEN;
	req.sockfd = sockfd;
	req.backlog = backlog;

	msgsend(NETD, &req, sizeof(Net_listen_req_t));
	msgrecv(NETD, &resp, sizeof(Net_listen_resp_t), &status);
	return resp.ret;
}

int accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen) {
	Net_accept_req_t req;
	Net_accept_resp_t resp;
	Msg_status_t status;

	req.type = NET_ACCEPT;
	req.sockfd = sockfd;

	if (cliaddr == NULL) {
		req.copy_addr = 0;
		req.addrlen = 0;
	} else {
		req.copy_addr = 1;
		req.addrlen = *addrlen;
	}

	msgsend(NETD, &req, sizeof(Net_accept_req_t));
	msgrecv(NETD, &resp, sizeof(Net_accept_resp_t), &status);

	if (resp.ret < 0)
		return resp.ret;

	/* FIXME: Buffer overflow */
	if (cliaddr != NULL) {
		memcpy(cliaddr, &(resp.cliaddr), resp.addrlen);
		*addrlen = resp.addrlen;
	}

	return resp.ret;
}

//#define CONNECT_DEBUG 1

int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen) {
	Net_connect_req_t req;
	Net_connect_resp_t resp;
	Msg_status_t status;
	struct sockaddr_in *tmp;
	struct sockaddr tmp2;

#ifdef CONNECT_DEBUG 
	printf("enter connect\n");
#endif	/* CONNECT_DEBUG */

	/* For some reason, the struct sockaddr->sa_family is set to zero
	 * This means we'll eat shit as soon as we compare again with
	 * AF_INET (#define 2)
	 * This is a HACK and definitely needs to be investigated!
	 */
	memcpy(&tmp2, serv_addr, sizeof(struct sockaddr));
	tmp2.sa_family = AF_INET;

	req.type = NET_CONNECT;
	req.sockfd = sockfd;
	memcpy(&(req.serv_addr), &tmp2, sizeof(struct sockaddr));
	req.addrlen = addrlen;

#ifdef CONNECT_DEBUG 
	printf("send netd\n");
#endif	/* CONNECT_DEBUG */
	msgsend(NETD, &req, sizeof(Net_connect_req_t));
#ifdef CONNECT_DEBUG 
	printf("recv netd\n");
#endif	/* CONNECT_DEBUG */
	msgrecv(NETD, &resp, sizeof(Net_connect_resp_t), &status);

#ifdef CONNECT_DEBUG 
	printf("exit connect\n");
#endif	/* CONNECT_DEBUG */

	return resp.ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
	Net_send_req_t req;
	Net_send_resp_t resp;
	Msg_status_t status;
	int sendlen, num_messages, i, initial_len;
	size_t remainder;
	ssize_t bytes_sent;

	num_messages=1;
	remainder=0;
	initial_len=0;
	bytes_sent=0;
	initial_len=len;
	remainder=len;

        /* Due to message passing constraints, we can only send/recv 
         * SOCKET_SNDBUF_MAX bytes at any given time. 
         * This implies that we need to truncate the message
         */

        if(len>SOCKET_SNDBUF_MAX)
        {
                /* How many messages do we need? */
                if((len%SOCKET_SNDBUF_MAX) == 0)
			num_messages = len/SOCKET_SNDBUF_MAX;
		else
			num_messages = ((len+SOCKET_SNDBUF_MAX-1)/SOCKET_SNDBUF_MAX);
		
		len = SOCKET_SNDBUF_MAX;
        }

	/* Send each part of the message individually */
	for(i=1; i<=num_messages; i++) {
		/* Configure the header */
		req.hdr.type = NET_SEND;
	        req.hdr.sockfd = sockfd;
		
		if(i==num_messages) /* Last message to send */
			len = remainder;
			
		req.hdr.len = len;
        	req.hdr.flags = flags;

        	memcpy(&(req.buf), buf, len);

        	sendlen = sizeof(Net_send_hdr_t) + len;
        	msgsend(NETD, &req, sendlen);
        	msgrecv(NETD, &resp, sizeof(Net_send_resp_t), &status);

		bytes_sent = bytes_sent + (size_t)resp.ret;

		remainder -= SOCKET_SNDBUF_MAX;
		buf += SOCKET_SNDBUF_MAX;
	}

	/* FIXME: Set errno */
	return bytes_sent;
}

ssize_t sendto(int sockfd, void *buf, size_t len, int flags,
					  struct sockaddr *dest_addr, socklen_t addrlen) {
	Net_send_req_t req;
	Net_send_resp_t resp;
	Msg_status_t status;
	int sendlen;

#if 0 /* TODO: Check args */
	/* @todo: split into multiple sendto's? */
	LWIP_ASSERT("lwip_sendto: size must fit in u16_t", size <= 0xffff);
	short_size = (u16_t)size;
	LWIP_ERROR("lwip_sendto: invalid address", (((to == NULL) && (tolen == 0)) ||
             ((tolen == sizeof(struct sockaddr_in)) &&
             ((to->sa_family) == AF_INET) && ((((mem_ptr_t)to) % 4) == 0))),
             sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);
#endif

	/* TODO: Split large sends up into sizes that our msg passing can handle */
	/* TODO: Check addrlen before using in memcpy */

	req.hdr.type    = NET_SENDTO;
	req.hdr.sockfd  = sockfd;
	req.hdr.len     = len;
	req.hdr.flags   = flags;

	if (dest_addr) {
		req.hdr.addr_valid = 1;
		req.hdr.addrlen = addrlen;
		memcpy(&(req.hdr.dst), dest_addr, addrlen);		
	} else {
		req.hdr.addr_valid = 0;
	}

	memcpy(&(req.buf), buf, len);

	sendlen = sizeof(Net_send_hdr_t) + len;

	msgsend(NETD, &req, sendlen);
	msgrecv(NETD, &resp, sizeof(Net_send_resp_t), &status);

	/* FIXME: Set errno */
	return resp.ret;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
						struct sockaddr *src_addr, socklen_t *addrlen) {
	Net_recv_req_t req;
	Net_recv_resp_t resp;
	Msg_status_t status;
	int recvlen;

	req.type = NET_RECVFROM;
	req.sockfd = sockfd;
	req.len = len;
	req.flags = flags;

	recvlen = sizeof(Net_recv_hdr_t) + len;

	msgsend(NETD, &req, sizeof(Net_recv_req_t));
	msgrecv(NETD, &resp, recvlen, &status);

	if (resp.hdr.ret > 0) {
		memcpy(buf, resp.buf, resp.hdr.ret);
	}

	if (src_addr != NULL) {
		uint32_t cp_len;
		cp_len = (resp.hdr.addrlen < (*addrlen)) ? resp.hdr.addrlen : *addrlen;
		memcpy(src_addr, &(resp.hdr.src), cp_len);
		*addrlen = cp_len;
	}

	return resp.hdr.ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
	Net_recv_req_t req;
	Net_recv_resp_t resp;
	Msg_status_t status;
	int recvlen;

	req.type = NET_RECV;
	req.sockfd = sockfd;
	req.len = len;
	req.flags = flags;

	recvlen = sizeof(Net_recv_hdr_t) + len;

	msgsend(NETD, &req, sizeof(Net_recv_req_t));
	msgrecv(NETD, &resp, recvlen, &status);

	if (resp.hdr.ret > 0) {
		memcpy(buf, resp.buf, resp.hdr.ret);
	}

	return resp.hdr.ret;
}

struct hostent *gethostbyname(const char *name) {
	return NULL;
}


int update_owner(int sockfd, int new_pid){
	Net_update_owner_req_t req;
	Net_update_owner_resp_t resp;
	Msg_status_t status;

	req.type = NET_UPDATE;
	req.sockfd = sockfd;
	req.pid = new_pid;

	msgsend(NETD, &req, sizeof(Net_update_owner_req_t));
	msgrecv(NETD, &resp, sizeof(Net_update_owner_resp_t), &status);

	return resp.ret;
}

