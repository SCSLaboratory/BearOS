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
/*******************************************************************************
 *
 * File: netd.c
 *
 * Description: Userland network task.  Manages sockets, does TCP/UDP, etc.
 *
 ******************************************************************************/

#include <stdint.h>
#include <malloc.h>
#include <msg.h>
#include <msg_types.h>

#include <stdio.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <utils/queue.h>

/* The network daemon uses lwip directly */
#include <sbin/lwip/sockets.h>
#include <sbin/lwip/netdb.h>		
#include <sbin/lwip/netif.h>
#include <sbin/lwip/init.h>
#include <sbin/lwip/ip_addr.h>
#include <sbin/lwip/etharp.h>
#include <sbin/lwip/dhcp.h>

#include <sbin/netd.h>
#include <sbin/bsock.h>
#include <sbin/driver_api.h>
#include <sbin/static_ip.h>

/* Array of ifp's for quick lookup based on ifnum. */
#define NETD_MAX_IFS 5
static struct netif *ifp_array[NETD_MAX_IFS];

static int netd_is_up = 0;
static int number_of_cards =0;
uint32_t packets_in = 0;
/* Poll queue. */
static queue_t *pollq;

static void net_init();
static void loopif_init();
static void net_main_loop();
static int  net_mac2ip(uint8_t *hwaddr, ip_addr_t *ip, ip_addr_t *nm, ip_addr_t *gw);
static int  net_maccmp(uint8_t *hwaddr1, uint8_t *hwaddr2);
static int  net_is_dup_if(pid_t pid, struct netif **pifp);
static int poll_check(Net_poll_t *proc);
static int poll_search(void *elementp, const void *keyp);
static void poll_tick();

/* 
 * Types of messages that may be received. This union provides the minimum
 * buffer size necessary to receive any valid message.
 */
typedef union {
	Net_ifadd_req_t   ifadd_msg;         /* From NIC driver - new card!      */
	Link_status_msg_t link_msg;          /* From NIC driver - link status    */
	Pkt_msg_t         incoming_pkt_msg;  /* From NIC driver - new pkt        */
	Net_timer_msg_t   timer_msg;         /* From kernel     - timer expired  */
	Ifconfig_req_t    ifconfig_msg;      /* From user prog  - ifconfig       */

	Net_socket_req_t  socket_msg;        /* From user prog  - socket()       */
	Net_bind_req_t    bind_msg;          /* From user prog  - bind()         */
	Net_listen_req_t  listen_msg;        /* From user prog  - listen()       */
	Net_connect_req_t connect_msg;       /* From user prog  - connect()      */
	Net_accept_req_t  accept_msg;        /* From user prog  - accept()       */
	Net_send_req_t    send_msg;          /* From user prog  - send()         */
	Net_recv_req_t    recv_msg;          /* From user prog  - recv()         */
	Net_ghbn_req_t    ghbn_msg;          /* From user prog  - gethostbyname()*/
	Net_close_req_t   close_req;         /* From user prog  - close()        */
	Net_update_owner_req_t up_owner_req;  /*From user program to update new socket pid owner  */
	Net_poll_t    poll_msg;          /* From kernel - start poll() */
	Netd_ping_req_t ping_msg;
} Net_msg_t;

static void net_do_invalid (Net_msg_t*, Msg_status_t*);
static void net_do_ifadd   (Net_msg_t*, Msg_status_t*);
static void net_do_link    (Net_msg_t*, Msg_status_t*);
static void net_do_inpkt   (Net_msg_t*, Msg_status_t*);
static void net_do_timer   (Net_msg_t*, Msg_status_t*);
static void net_do_ifconfig(Net_msg_t*, Msg_status_t*);

static void net_do_socket  (Net_msg_t*, Msg_status_t*);
static void net_do_bind    (Net_msg_t*, Msg_status_t*);
static void net_do_listen  (Net_msg_t*, Msg_status_t*);
static void net_do_connect (Net_msg_t*, Msg_status_t*);
static void net_do_accept  (Net_msg_t*, Msg_status_t*);
static void net_do_send    (Net_msg_t*, Msg_status_t*);
static void net_do_sendto  (Net_msg_t*, Msg_status_t*);
static void net_do_recv    (Net_msg_t*, Msg_status_t*);
static void net_do_recvfrom(Net_msg_t*, Msg_status_t*);
static void net_do_ghbn    (Net_msg_t*, Msg_status_t*);
static void net_do_close   (Net_msg_t*, Msg_status_t*);
static void net_do_update  (Net_msg_t*, Msg_status_t*);
static void net_do_poll    (Net_msg_t*, Msg_status_t*);

static void netd_do_ping	 (Net_msg_t*, Msg_status_t*);
#define NET_FUNC_ARRAY_SZ 20

/* CAUTION: Order of these functions must match numbers defined in kmsg.h */
static void (*func_array[NET_FUNC_ARRAY_SZ])(Net_msg_t*, Msg_status_t*) = 
{ net_do_invalid, net_do_ifadd,    net_do_link,   net_do_inpkt,
  net_do_timer,   net_do_ifconfig, net_do_socket, net_do_bind,
  net_do_listen,  net_do_connect,  net_do_accept, net_do_send,
  net_do_sendto,  net_do_recv,     net_do_recvfrom, net_do_ghbn,
  net_do_close,   net_do_update,   net_do_poll, netd_do_ping
};

/* Start function. */
int main() {
  int mypid;
  mypid = getpid();
  if(mypid!=NETD) {
    printf("[netd: initialized with pid %d, expected %d\n",mypid,NETD);
    exit(EXIT_FAILURE);
  }
  net_init();
  net_main_loop();
}

/* PRIVATE FUNCTIONS */

static void net_init() {
	memset(ifp_array,0,sizeof(ifp_array));
	pollq = qopen();
	bsock_init();
	lwip_init();
	loopif_init();
}

static void net_main_loop() {
	Net_msg_t msg;
	Msg_status_t status;
	int *ptype;
	int ret, i=0;

	ptype = (int*)(&msg);


	while(1) {

		ret = msgrecv(ANY, &msg, sizeof(Net_msg_t), &status);

		if (ret >= 0) {
			if ((*ptype) >= 0 && (*ptype) < NET_FUNC_ARRAY_SZ) {
				(*func_array[*ptype])(&msg,&status);
			} else {
				net_do_invalid(&msg,&status);
			}
		}
		//if(!(i%10))
			poll_tick();
		//i++;
	}
}

/* Called if a message arrives with an invalid type. */
static void net_do_invalid (Net_msg_t *msg, Msg_status_t *status) {
	printf("[NET] ERROR: Invalid msg type = %d src = %d len = %d\n",*((int*)msg),status->src, status->msgsize_orig);
}

static void loopif_init(){

	struct netif *ifp;
	
	ifp = netif_find("lo0");
	
	if(ifp != NULL){
		ifp_array[ifp->num] = ifp;
		number_of_cards += 1;
	}
	else
		printf("No loop interace found \n");
}
		
/* Notification of a new network interface. */
static void net_do_ifadd(Net_msg_t *msg, Msg_status_t *status) {
	ip_addr_t ipaddr, netmask, gw;
	Driver_api_softc_t *sc;
	struct netif *ifp;
	int ret;
	int ifnum;

	ipaddr.addr = 0;
	netmask.addr = 0;
	gw.addr = 0;
	
	/* 
	 * Check to see if this is a refreshed driver re-registering.
	 */
	if (net_is_dup_if(status->src, &ifp)) {

		/* Ok, still need to initialize the driver process. */
		ret = driver_init(ifp);

		/* 
		 *  Bring interface down and back up, since that's essentially what
		 * happened to the link layer. Doing this sends will announce our
		 * presence on the network with a gratuitous ARP message.  Without
		 * this, the switch doesn't know we're there and we get no traffic.
		 */
		netif_set_link_down(ifp);
		netif_set_link_up(ifp);

		return;
	}

	/* Allocate a little driver structure */
	sc = malloc(sizeof(Driver_api_softc_t));
	if (sc == NULL) {
		printf("[NET] ERROR: could not allocate Driver_api_softc_t.\n");
		return;
	}

	/* Allocate a netif struct */
	ifp = malloc(sizeof(struct netif));
	if (ifp == NULL) {
		printf("[NET] ERROR: could not allocate struct netif.\n");
		return;
	}

	/* Init driver struct */
	sc->pid = status->src;
	sc->ifp = ifp;

	/* Add new if */

	netif_add(sc->ifp, &ipaddr, &netmask, &gw, sc, &driver_init, &ethernet_input);

	/* Figure out the IP address situation for this card. */
	ret = net_mac2ip(ifp->hwaddr, &ipaddr, &netmask, &gw);
	if (ret < 0) {
		/* This MAC isn't in our static IP list.  Just do DHCP */
		dhcp_start(ifp);
	} else {
		/* Give ourselves the static IP. */
		netif_set_addr(ifp, &ipaddr, &netmask, &gw);
		netif_set_up(ifp);
		netif_set_default(ifp); /* We know this is a good connection */
	}

	/* Add to array */
	if ((ifp->num >= NETD_MAX_IFS) || (ifp->num < 0)) {
		printf("[NET] ERROR: Invalid if number.  Too many IF's?\n");
		return;
	}
	number_of_cards += 1;
	ifp_array[ifp->num] = ifp;
}

static int net_mac2ip(uint8_t *hwaddr, ip_addr_t *ip, ip_addr_t *nm, ip_addr_t *gw) {
	int i;

	for(i=0; i<STATIC_IP_TABLE_SIZE; i++) {
		if (net_maccmp(hwaddr, static_ip_table[i].hwaddr)) {
			if (ip != NULL) {
				memcpy(ip, &(static_ip_table[i].ip), sizeof(ip_addr_t));
			}
			if (nm != NULL) {
				memcpy(nm, &(static_ip_table[i].nm), sizeof(ip_addr_t));
			}
			if (gw != NULL) {
				memcpy(gw, &(static_ip_table[i].gw), sizeof(ip_addr_t));
			}
			return 0;
		}
	}
	return -1;
}

static int net_maccmp(uint8_t *hwaddr1, uint8_t *hwaddr2) {
	int i;
	for (i=0; (hwaddr1[i] == hwaddr2[i]) && (i < 6); i++);
	return (i==6);
}

static void net_do_link(Net_msg_t *msg, Msg_status_t *status) {
	struct netif *ifp;
	Driver_api_softc_t *sc;
	Link_status_msg_t *lmsg;

	lmsg = (Link_status_msg_t *)msg;

	/* Get ifp from array */
	ifp = ifp_array[lmsg->ifnum];
	if (ifp == NULL) {
		printf("[NET] Error: Got link information for unknown ifp in do link??\n");
		return;
	}

	/* Check to see if the PID matches up (no spoofed messages) */
	sc = (Driver_api_softc_t *)ifp->state;
	if (sc == NULL) {
		printf("[NET] Error: No driver state for ifp %d\n", lmsg->ifnum);
		return;
	}
	if (sc->pid != status->src) {
		printf("WARNING: Found spoofed message! from %d expected %d\n", status->src, sc->pid);
		return;
	}

	/* Update link status */
	if (lmsg->status)
		netif_set_link_up(ifp);
	else
		netif_set_link_down(ifp);
}

static void net_do_inpkt(Net_msg_t *msg, Msg_status_t *status) {
	Pkt_msg_t *pmsg;
	Driver_api_softc_t *sc;
	struct netif *ifp;
	struct pbuf *p;

	pmsg = (Pkt_msg_t *)msg;
	packets_in++;
//printf("netd recievecd a packet %d packets\n", packets_in);
	/* Get ifp from array */
	ifp = ifp_array[pmsg->ifnum];
	if (ifp == NULL) {
		printf("[NET] Error: Got link information for unknown ifp in do inpkt??\n");
		return;
	}

	/* Check to see if the PID matches up (no spoofed messages) */
	sc = (Driver_api_softc_t *)ifp->state;
	if (sc->pid != status->src) {
		printf("WARNING: Found spoofed message! from %d expected %d\n", status->src, sc->pid);
		return;
	}

	/* Put into a pbuf */
	p = pbuf_alloc(PBUF_IP, pmsg->len, PBUF_POOL);
	if (p == NULL) {
		printf("[NET] Warning: Packet dropped! No pbufs left.\n");
		return;
	}
	memcpy(p->payload, pmsg->pkt, pmsg->len);

	/* Pass to lwIP */
	if( ifp->input(p, ifp) != ERR_OK ) {
		pbuf_free(p);
	}
}

static void net_do_timer(Net_msg_t *msg, Msg_status_t *status) {
	Net_timer_msg_t *tmsg;

	tmsg = (Net_timer_msg_t *)msg;

	switch(tmsg->timer) {
	case NET_ARP_TMR:
		etharp_tmr();
		break;
	case NET_DHCP_COARSE_TMR:
		dhcp_coarse_tmr();
		break;
	case NET_DHCP_FINE_TMR:
		dhcp_fine_tmr();
		break;
	case NET_TCP_TMR:
		tcp_tmr();
		break;
	}

}

static void net_do_ifconfig(Net_msg_t *msg, Msg_status_t *status) {
	Ifconfig_req_t *req;
	Ifconfig_resp_t resp;
	struct netif *ifp;
	int i;
	req = (Ifconfig_req_t *)msg;
	char *name; 
	char *integer_string; 

	memset(&resp,0,sizeof(Ifconfig_resp_t));
	resp.type = NET_IFCONFIG;
		
	if( req->interface[0] == '\0'){				/* get all cards */
		for(i=0; i< number_of_cards; i++){
			printf("Interface %s%d \n", ifp_array[i]->name, ifp_array[i]->num);
			printf("Ip address %s\n", inet_ntoa(ifp_array[i]->ip_addr) );
			printf("netmask  %s \n", inet_ntoa(ifp_array[i]->netmask));
			printf("gateway %s \n", inet_ntoa(ifp_array[i]->gw));	
		}
		resp.ret = 0;
	}else{		/* Find specified interface */
		if((req->number > number_of_cards-1) || (req->number < 0)){
			printf("card does not exist \n");
			resp.ret = -1;
		}else{
			printf("Interface %s%d \n", ifp_array[req->number]->name, ifp_array[req->number]->num);
			printf("Ip address %s\n", inet_ntoa(ifp_array[req->number]->ip_addr) );
			printf("netmask  %s \n", inet_ntoa(ifp_array[req->number]->netmask));
			printf("gateway %s \n", inet_ntoa(ifp_array[req->number]->gw));	
			resp.ret = 0;
		}	
	}
	msgsend(status->src, &resp, sizeof(Ifconfig_resp_t));	
}
	
/* SOCKET CALLS */
static void net_do_socket(Net_msg_t *msg, Msg_status_t *status) {
	bsock_socket((Net_socket_req_t *)msg, status);
}

static void net_do_bind(Net_msg_t *msg, Msg_status_t *status) {
	bsock_bind((Net_bind_req_t *)msg, status);
}

static void net_do_listen(Net_msg_t *msg, Msg_status_t *status) {
	bsock_listen((Net_listen_req_t *)msg, status);
}

static void net_do_connect(Net_msg_t *msg, Msg_status_t *status) {
	bsock_connect((Net_connect_req_t *)msg, status);
}

static void net_do_accept(Net_msg_t *msg, Msg_status_t *status) {
	bsock_accept((Net_accept_req_t *)msg, status);
}

static void net_do_send(Net_msg_t *msg, Msg_status_t *status) {
	bsock_send((Net_send_req_t *)msg, status);
}

static void net_do_sendto(Net_msg_t *msg, Msg_status_t *status) {
	bsock_sendto((Net_send_req_t *)msg, status);
}

static void net_do_recv(Net_msg_t *msg, Msg_status_t *status) {
	bsock_recv((Net_recv_req_t *)msg, status);
}

static void net_do_recvfrom(Net_msg_t *msg, Msg_status_t *status) {
	bsock_recvfrom((Net_recv_req_t *)msg, status);
}

static void net_do_ghbn(Net_msg_t *msg, Msg_status_t *status) {
	bsock_ghbn((Net_ghbn_req_t *)msg, status);
}

static void net_do_close(Net_msg_t *msg, Msg_status_t *status) {
	bsock_close((Net_close_req_t *)msg, status);
}

static void net_do_update(Net_msg_t *msg, Msg_status_t *status) {
	bsock_update_owner((Net_update_owner_req_t *)msg, status);
}

static void net_do_poll(Net_msg_t *msg, Msg_status_t *status) {
	Net_poll_t *req = (Net_poll_t *)msg;
	Net_poll_t *copy;
	int i;
	struct timespec tp;

	if(qsearch(pollq, poll_search, &status->src) != NULL) {
		/* Serious error -- there's already an ongoing poll! */
		req->rval = -1;
		req->errno = -1; /* TODO EUNKNOWN */
		goto out;
	}

	if(!(req->numfds >= 0)) {
		/* The client sent us a bad value. */
		req->rval = 0;
		goto out;
	}

	/* Check if there's already data waiting. If so, we need to return
	 * right now. */
	for(i = 0, req->rval = 0; i < req->numfds; i++) {
		int set = 0;
		if((req->fds[i].events & POLLIN) &&
		   bsock_has_data(req->fds[i].fd)) {
			req->fds[i].revents |= POLLIN;
			set = 1;
		}
		if(req->fds[i].events & POLLOUT) {
			// FIXME: We say we're always ready to write.
			req->fds[i].revents |= POLLOUT;
			set = 1;
		}
		if(bsock_is_closed(req->fds[i].fd)) {
			req->fds[i].revents = POLLHUP;
			set = 1;
		}
		if(set)
			req->rval++;
	}
	if(req->rval) goto out;

	/* If we made it this far, good to go with the polling. */
	req->pid = status->src;
	copy = malloc(sizeof(Net_poll_t));
	memcpy(copy, req, sizeof(Net_poll_t));


	/* Set timer. Note that a timeout of 0 means that the polled fds MUST
	 * be checked once. Thus, they get poll-enqueued anyway, since the
	 * timer is checked AFTER the fds are checked. */
	if(req->timeout < 0) {
		copy->timeout = 0;
	} else {
		copy->timeout = (clock_gettime(4, &tp) * 1000 ) + req->timeout;
	}

	qput(pollq, copy);
	
	/* Don't send a return message yet. A return message will cause the
	 * process to wake up, which we can't do until timeout or when polled
	 * data arrives. */
	return;

	out:
	msgsend(status->src, req, sizeof(Net_poll_t));
}

static int poll_check(Net_poll_t *proc) {
	int i;
	int revents;
	struct timespec tp;

	/* Any revents? */
	for(i = 0, revents = 0; i < proc->numfds; i++) {
		int set = 0;
		if((proc->fds[i].events & POLLIN) && bsock_has_data(proc->fds[i].fd)) {
			proc->fds[i].revents |= POLLIN;
			set = 1;
		}
		if(proc->fds[i].events & POLLOUT) {
			// FIXME: Always ready to write.
			proc->fds[i].revents |= POLLOUT;
			set = 1;
		}
		if(bsock_is_closed(proc->fds[i].fd)) {
			proc->fds[i].revents = POLLHUP;
			set = 1;
		}
		if(set)
			revents++;
	}
	if(revents) {
		proc->rval = revents;
		return 1;
	}

	/* Did we timeout? */
	if(proc->timeout > 0 &&
	   (clock_gettime(4, &tp) * 1000 ) > proc->timeout) {
		return 1;
	}

	
	/* If we made it here, nothing to report. */
	return 0;
}

static int poll_search(void *elementp, const void *keyp) {
  Net_poll_t *ep;
  int *kp;

  ep = (Net_poll_t*)(elementp);
  kp = (int*)keyp;
  return(ep->pid == *kp);
}

/* poll_tick: check to see if the processes have polled data in their
 *            examined fds or timed out. This is hooked into the netd
 *            main loop. */
static void poll_tick() {
	/* We can't use qapply here because you can't qremove during a qapply.
	 * Thus, we need to pull from / put back into the queue until we find
	 * the first element again, which marks a complete cycle. */
	Net_poll_t *orig;
	Net_poll_t *current;

	orig = qget(pollq);
	if(orig == NULL) return;
	qput(pollq, orig);
	while((current = qget(pollq)) != orig) {
		if(!poll_check(current)) {
			qput(pollq, current);
		} else {
			msgsend(current->pid, current, sizeof(Net_poll_t));
			free(current);
		}
	}
	if(!poll_check(orig)) {
		qput(pollq, orig);
	} else {
		msgsend(orig->pid, orig, sizeof(Net_poll_t));
		free(orig);
	}
}

static int net_is_dup_if(pid_t pid, struct netif **pifp) {
	struct netif *ifp;
	Driver_api_softc_t *sc;
	char i;
	char ifname[3];

	i = 0;
	ifname[0] = 'e'; ifname[1] = 't';

	/* 
	 * Search through all if's and see if there's already one
	 * associated with this process
	 */
	while(1) {
		ifname[2] = '0' + i; /* FIXME: only works up to et9 */
		ifp = netif_find(ifname);
		if (ifp == NULL)
			break;

		sc = (Driver_api_softc_t*)ifp->state;
		if (sc->pid == pid) {
			*pifp = ifp;
			return 1;
		}

		i++;
	}

	return 0;
}

void panic() {
	printf("[NET] PANIC!\n");
	exit(1);
}


void netd_do_ping(Net_msg_t *msg, Msg_status_t *status) {
	Netd_ping_resp_t resp;
	int src;
	struct netif *ifp;

	src = status->src;
	memcpy((void*)ifp, (void*)ifp_array[1], sizeof(struct netif));

	if(netif_is_up(ifp))
		netd_is_up = 1;

	resp.value = netd_is_up;
	msgsend(src, &resp, sizeof(Netd_ping_resp_t));
}
