#pragma once

#include <stdint.h>
#include <limits.h>
#include <sbin/syspid.h>
#include <sbin/pci_structs.h>

#define NET_ARP_TMR 0
#define NET_DHCP_COARSE_TMR 1
#define NET_DHCP_FINE_TMR 2
#define NET_TCP_TMR 3

#define NET_NUM_TIMERS 4

/* These go to/from the net task. */
/**** DEFINITIONS ****/
/* To net from driver */
#define NET_IFADD         1
#define NET_IFLINK        2
#define NET_PKT_INCOMING  3
#define NET_TIMER_MSG     4
/* To net from user procs */
#define NET_IFCONFIG      5
#define NET_SOCKET        6
#define NET_BIND          7
#define NET_LISTEN        8
#define NET_CONNECT       9
#define NET_ACCEPT       10
#define NET_SEND         11
#define NET_SENDTO       12
#define NET_RECV         13
#define NET_RECVFROM     14
#define NET_GHBN         15
#define NET_CLOSE        16
#define NET_UPDATE       17
#define NET_POLL         18
#define NET_PING	 19

/* These go to/from a network driver (for us, bced). */
/**** DEFINITIONS ****/
/* These go to the driver. */
#define NDRV_INIT      1  /* Init network card      */
#define NDRV_OUTPKT    2  /* Outgoing packet        */
#define NDRV_TIMER_MSG 3  /* Timer msg              */
#define NDRV_CONFIG    4  /* Config msg from kernel */

/* first third for files, second third for sockets, last third for pipes */
#define SOCKIDMIN ((INT_MAX/3)+1)
#define SOCKIDMAX (2*(INT_MAX/3))
#define is_sockid(x) (x>=SOCKIDMIN && x<=SOCKIDMAX)

/*** TO/FROM NET TASK ***/

typedef struct {
        int type;
} Net_ifadd_req_t;

typedef struct {
        int type;
        int ifnum;
        int status;
} Link_status_msg_t;

typedef struct {
        int type;
        int len;
        int ifnum;
} Pkt_hdr_t;

typedef struct {
        int type;
        int len;
        int ifnum;
        uint8_t pkt[1600];
} Pkt_msg_t;

typedef struct {
        int type;
        int timer;
} Net_timer_msg_t;

/*** TO/FROM NETWORK DRIVER ***/
typedef struct {
        int type;
        Pci_dev_t dev; /*Pci_dev_t */
        void *mmio_addr;
        void *flash_addr;
        void *dma_vaddr;
        void *dma_paddr;
        uint64_t dma_sz;
} Ndrv_config_msg_t;


#ifdef USER

/* ifconfig */
typedef struct {
        int type;
        char interface[3];
				int number; 
} Ifconfig_req_t;

typedef struct {
        int type;
        int ret;
} Ifconfig_resp_t;

/* SOCKET API */

/* socket() */
typedef struct {
        int type;
        int domain;
        int stype;
        int protocol;
} Net_socket_req_t;

typedef struct {
        int type;
        int sockfd;
        int errno;
} Net_socket_resp_t;

/* bind() */
typedef struct {
        int type;
        int sockfd;
        struct sockaddr my_addr;
        socklen_t addrlen;
} Net_bind_req_t;

typedef struct {
        int type;
        int ret;
        int errno;
} Net_bind_resp_t;

/* listen() */
typedef struct {
        int type;
        int sockfd;
        int backlog;
} Net_listen_req_t;

typedef struct {
        int type;
        int ret;
        int errno;
} Net_listen_resp_t;

/* connect() */
typedef struct {
        int type;
        int sockfd;
        struct sockaddr serv_addr;
        socklen_t addrlen;
} Net_connect_req_t;

typedef struct {
        int type;
        int ret;
        int errno;
} Net_connect_resp_t;

/* accept() */
typedef struct {
        int type;
        int sockfd;
        int copy_addr;
        socklen_t addrlen;
} Net_accept_req_t;

typedef struct {
        int type;
        struct sockaddr cliaddr;
        socklen_t addrlen;
        uint16_t port;
        uint32_t ip_addr;
        int ret;
        int errno;
} Net_accept_resp_t;

/* send */
typedef struct {
        int type;
        int sockfd;
        size_t len;
        int flags;
        struct sockaddr dst;
        socklen_t addrlen;
        int addr_valid;
        /* data goes here */
} Net_send_hdr_t;

typedef struct {
        Net_send_hdr_t hdr;
        uint8_t buf[SOCKET_SNDBUF_MAX];
} Net_send_req_t;

typedef struct {
        int type;
        ssize_t ret;
        int errno;
} Net_send_resp_t;

/* recv */
typedef struct {
        int type;
        int sockfd;
        size_t len;
        int flags;
} Net_recv_req_t;

typedef struct {
        int type;
        int sockfd;
        ssize_t ret;
        int errno;
        struct sockaddr src;
        socklen_t addrlen;
} Net_recv_hdr_t;

typedef struct {
        Net_recv_hdr_t hdr;
        uint8_t buf[SOCKET_SNDBUF_MAX];
} Net_recv_resp_t;

/* gethostbyname() */
#define NET_NAME_LEN 64
typedef struct {
        int type;
        char name[NET_NAME_LEN];
} Net_ghbn_req_t;

typedef struct {
        int type;
        struct hostent host;
        int ret;
        int h_errno;
} Net_ghbn_resp_t;

/* close() */
typedef struct {
        int type;
        int sockfd;
} Net_close_req_t;

typedef struct {
        int type;
        int ret;
        int errno;
} Net_close_resp_t;

/* poll(); request/response are same messages */
#include <poll.h> /* For struct pollfd */

typedef struct {
	int type;
	int pid;
	struct pollfd fds[100]; /* A reasonable limit for now */
	int numfds;
	int timeout;
	int rval;
	int errno;
} Net_poll_t;

/* update_owner() */
typedef struct {
        int type;
        int sockfd;
        int pid;
} Net_update_owner_req_t;

typedef struct {
        int type;
        int errno;
        int ret;
} Net_update_owner_resp_t;


typedef struct {
        int type;
        int ifnum;
} Ndrv_init_req_t;

typedef struct {
        int type;
        int ifnum;
        int ret;
        uint8_t eaddr[6];
} Ndrv_init_resp_t;

typedef struct {
        int type;
        int alarm_type;
} Ndrv_alarm_msg_t;

typedef struct {
	int type;
} Netd_ping_req_t;

typedef struct {
	int type;
	int value;
} Netd_ping_resp_t;


#endif /* USER */
