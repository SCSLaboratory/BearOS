#pragma once
/*******************************************************************************
 *
 * File: bsock.h
 *
 * Description: The API for the bear implementation of sockets.
 *
 ******************************************************************************/

/*These are the socket messages that will be sent to the network daemon*/

#include <sbin/netd.h>	/* Communicating with the netword daemon */
#include <msg.h>

void bsock_init();

void bsock_socket(Net_socket_req_t *req, Msg_status_t *status);
void bsock_bind(Net_bind_req_t *req, Msg_status_t *status);
void bsock_listen(Net_listen_req_t *req, Msg_status_t *status);
void bsock_connect(Net_connect_req_t *req, Msg_status_t *status);
void bsock_accept(Net_accept_req_t *req, Msg_status_t *status);
void bsock_send(Net_send_req_t *req, Msg_status_t *status);
void bsock_sendto(Net_send_req_t *req, Msg_status_t *status);
void bsock_recv(Net_recv_req_t *req, Msg_status_t *status);
void bsock_recvfrom(Net_recv_req_t *req, Msg_status_t *status);
void bsock_ghbn(Net_ghbn_req_t *req, Msg_status_t *status);
void bsock_close(Net_close_req_t *req, Msg_status_t *status);
void bsock_update_owner(Net_update_owner_req_t *req, Msg_status_t *status);

/* Direct function call stuff, because why the hell are we sending messages 
 * to ourselves again? */
int bsock_has_data(int sockfd);
int bsock_is_closed(int sockfd);
