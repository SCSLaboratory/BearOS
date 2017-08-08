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
 * File: bsock.c
 *
 * Description: The implementation of sockets used in Bear.  lwIP's
 *     socket impelmentation relies on threads; we don't have them. This is the
 *     replacement; it uses lwIP's raw API.
 *
 *   Basically what I've done here is mash up lwIP's api/socket.c and 
 *   api/api_msg.c (netconn implementation), and make the netconns use callbacks
 *   instead of waiting on semaphiores/mailboxes.
 *   Thus, a lot of this stuff relies on macros and definitions from lwIP.
 *   Most of the important definitions are here or in lwip/api.h.
 *
 ******************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <msg.h>      /* For msgsend and msgrecv       */
#include <stdint.h>

#include <utils/queue.h>
#include <utils/shash.h>
#include <utils/bool.h>

#include <sbin/lwip/sockets.h>
#include <sbin/lwip/netdb.h>	
#include <sbin/lwip/api.h>
#include <sbin/lwip/err.h>
#include <sbin/lwip/raw.h>	/* For raw API raw_xxx functions */
#include <sbin/lwip/udp.h>	/* For raw API udp_xxx functions */
#include <sbin/lwip/tcp.h>	/* For raw API tcp_xxx functions */
#include <sbin/lwip/api_msg.h> /* For shutdown types            */
#include <sbin/lwip/memp.h>    /* For memp_malloc               */

#include <msg_types.h>   /* For socket API message types  */
#include <poll.h>    /* For poll msgs */
#include <time.h>    /* for CLOCK_MONOTONIC */

#include <sbin/bsock.h>

/*** PRIVATE DECLARATIONS ***/

/* Uncomment this for bsock debugging purposes. */
/* #define BSOCK_DEBUG   */

/* Enables debug prints if BSOCK_DEBUG is on. */
#ifdef BSOCK_DEBUG
#  include <lib/lib.h>
#  define BSOCK_LOG printf
#  define BSOCK_FUNC_ENTER() BSOCK_LOG("[BSOCK] %s(enter)\n",__FUNCTION__)
#  define BSOCK_FUNC_EXIT()  BSOCK_LOG("[BSOCK] %s(exit)\n",__FUNCTION__)
#else
#  define BSOCK_LOG(...)
#  define BSOCK_FUNC_ENTER()
#  define BSOCK_FUNC_EXIT()
#endif /* BSOCK_DEBUG */
static void print_timestamp();

struct bsock_struct;

/* Used for select() */
/** A callback prototype to inform about events for a netconn */
typedef void (* bsock_callback)(struct bsock_struct *, enum netconn_evt, u16_t len);

/* a hashtable of sockets */
static shashtable_t *opensockets;
static int nextsockfd;

static int lookup(void* ep,const void *skeyp);

/* socket sturcture. */
typedef struct bsock_struct {
  int sockfd;                /* sockfd for this connection. */
  int pid;                   /* Owning process. */
  int active;                /* Is this socket/sockfd currently in use. */
  enum netconn_type  stype;  /* TCP, UDP, or raw */
  enum netconn_state state;  /* Netconn var - used in implementation */
  
  int blocked_on;            /* What syscall is this sock blocked on */
#define BSOCK_BL_NONE 0
#define BSOCK_BL_SOCKET 1
#define BSOCK_BL_BIND 2
#define BSOCK_BL_LISTEN 3
#define BSOCK_BL_CONNECT 4
#define BSOCK_BL_ACCEPT 5
#define BSOCK_BL_SEND 6
#define BSOCK_BL_RECV 7
#define BSOCK_BL_GHBN 8
#define BSOCK_BL_CLOSE 9
  
  int shutdown_state;        /* when closing, where are we at. */
  void *conn;    /* void pointer to a tcp_pcb, udp_pcb, or raw_pcb */
  err_t last_err;  /* Last error this connection experienced */
  uint8_t flags; /* flags for various options; see NETCONN_FLAG_* defines */
  
  /* For listen/accept */
  int aq_valid;        /* Is the accept queue open/valid    */
  queue_t *aq;            /* Queue of pending connections       */
  int aq_len;          /* Length of accept queue            */
  int copy_addr;       /* does user want new client's addr? */
  socklen_t addrlen;   /* How much space for addr           */
  
  /* For sending data */
  size_t write_offset;     /* offset into buffer for unsent data   */
  uint8_t write_flags;     /* Flags given by user to socket send() */
  int write_len;           /* len given by user to socket send()   */
  void *write_buf;         /* Internal buffer of data              */
  
  /* For receiving data */
  int rq_valid;        /* Is the receive queue valid                */
  queue_t *rq;            /* queue of received packets                 */
  size_t rq_len;    /* How much data is waiting in the queue     */
  uint32_t last_off;   /* Offset to unused data in earliest packet  */
  size_t recv_len;     /* How much data did the caller ask for      */
  int recv_flags;      /* What flags did the caller give            */
  
  bsock_callback callback; /* Used to implement select() */
} bsock_t;

typedef struct {
  bsock_t *sock;
  Net_recv_resp_t *resp;
  int recvlen;
  int bytes_copied;
  int pbufs_to_dequeue;
  int last_off;
} bsock_copy_info_t;

/* Macros for accessing various parts of the socket struct. */
#define tcp_conn(sock) ((struct tcp_pcb *)((sock)->conn))
#define udp_conn(sock) ((struct udp_pcb *)((sock)->conn))
#define raw_conn(sock) ((struct raw_pcb *)((sock)->conn))
#define ip_conn(sock)  ((struct ip_pcb  *)((sock)->conn))

#define SET_NONBLOCKING_CONNECT(conn, val)  do { if(val) {	\
      (conn)->flags |= NETCONN_FLAG_IN_NONBLOCKING_CONNECT;		\
    } else {								\
      (conn)->flags &= ~ NETCONN_FLAG_IN_NONBLOCKING_CONNECT; }} while(0)
#define IN_NONBLOCKING_CONNECT(conn) (((conn)->flags & NETCONN_FLAG_IN_NONBLOCKING_CONNECT) != 0)

#define bsock_set_nonblocking(conn, val) netconn_set_nonblocking(conn, val)
#define bsock_is_nonblocking(conn)       netconn_is_nonblocking(conn)

/** Table to quickly map an lwIP error (err_t) to a socket error
 * by using -err as an index */
static const int err_to_errno_table[] = {
  0,             /* ERR_OK          0      No error, everything OK. */
  ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
  ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
  EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
  EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
  EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
  EINVAL,        /* ERR_VAL        -6      Illegal value.           */
  EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */
  EADDRINUSE,    /* ERR_USE        -8      Address in use.          */
  EALREADY,      /* ERR_ISCONN     -9      Already connected.       */
  ECONNABORTED,  /* ERR_ABRT       -10     Connection aborted.      */
  ECONNRESET,    /* ERR_RST        -11     Connection reset.        */
  ENOTCONN,      /* ERR_CLSD       -12     Connection closed.       */
  ENOTCONN,      /* ERR_CONN       -13     Not connected.           */
  EIO,           /* ERR_ARG        -14     Illegal argument.        */
  -1,            /* ERR_IF         -15     Low-level netif error    */
};

#define ERR_TO_ERRNO_TABLE_SIZE					\
  (sizeof(err_to_errno_table)/sizeof(err_to_errno_table[0]))

#define err_to_errno(err)			  \
  ((unsigned)(-(err)) < ERR_TO_ERRNO_TABLE_SIZE ? \
   err_to_errno_table[-(err)] : EIO)


static int      bsock_validate_sock(bsock_t *sock, pid_t pid);
static int      bsock_alloc_sockfd(void *conn, int accepted, int pid, bsock_t **sockp);
static void     bsock_free_sockfd(int sockfd);
static bsock_t *bsock_get_sock(int i);
static err_t    bsock_new_tcp(bsock_t *sock);
static err_t    bsock_new_udp(bsock_t *sock);
static err_t    bsock_new_raw(bsock_t *sock, int protocol);
static err_t    bsock_do_connect(bsock_t *sock, ip_addr_t *ipaddr, uint16_t port);
static err_t    bsock_do_connected(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t    bsock_do_recvfrom(bsock_t *sock, size_t len, int flags);
static err_t    bsock_poll_tcp(void *arg, struct tcp_pcb *pcb);
static err_t    bsock_sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len);
static void     bsock_err_tcp(void *arg, err_t err);
static void     bsock_setup_tcp(bsock_t *sock);
static void     bsock_event_callback(bsock_t *sock, enum netconn_evt evt, uint16_t len);
static err_t    bsock_do_accept(bsock_t *sock);
static err_t    bsock_accept_function(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t    bsock_recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void     bsock_recv_udp(void *arg, struct udp_pcb *pcb, struct pbuf *p,
			       ip_addr_t *addr, u16_t port);
static uint8_t  bsock_recv_raw(void *arg, struct raw_pcb *pcb, struct pbuf *p,
			       ip_addr_t *addr);
static err_t bsock_do_sendto(bsock_t *sock, struct netbuf *buf);
static err_t bsock_do_write(bsock_t *sock, void *dataptr, size_t size, uint8_t apiflags);
static err_t bsock_do_writemore(bsock_t *sock);
static err_t bsock_do_delconn(bsock_t *sock);
static void bsock_drain(bsock_t *sock);
static void bsock_free_rq(bsock_t *sock);
static void bsock_free_aq(bsock_t *sock);
static void bsock_free_rq_tcp(void *arg, void *elementp);
static void bsock_free_rq_udp(void *elementp);
static void bsock_close_internal(bsock_t *conn);

static void bsock_wakeup        (bsock_t *sock, err_t err);
static void bsock_wakeup_listen (bsock_t *sock, err_t err);
static void bsock_wakeup_close  (bsock_t *sock, err_t err);
static void bsock_wakeup_connect(bsock_t *sock, err_t err);
static void bsock_wakeup_accept (bsock_t *sock, err_t err);
static void bsock_wakeup_send   (bsock_t *sock, err_t err);
static void bsock_wakeup_recv   (bsock_t *sock, err_t err);
static void bsock_wakeup_ghbn   (bsock_t *sock, err_t err);

static err_t bsock_getaddr(bsock_t *sock, ip_addr_t *addr, uint16_t *port, uint8_t local);
#define BSOCK_GETADDR_LOCAL 1  /* Don't change these or will break */
#define BSOCK_GETADDR_REMOTE 0 /* Don't change these or will break */
static int bsock_copy_recv(void *elementp, const void *keyp);


/*** PUBLIC FUNCTIONS ***/

#define MAXOPEN 50 		/* number of buckets in the hash table */

void bsock_init(void) {
  opensockets = shopen(MAXOPEN);
  nextsockfd=SOCKIDMIN;
}

void bsock_socket(Net_socket_req_t *req, Msg_status_t *status) {
  Net_socket_resp_t resp;
  bsock_t *sockp;
  int i;
  err_t err;
  
  resp.type = NET_SOCKET;
  
  if((i = bsock_alloc_sockfd(NULL, 0, status->src, &sockp))==ERR_MEM) {
    BSOCK_LOG("[bsock] socket array full\n");
    resp.sockfd = -1;
    resp.errno = err_to_errno(ERR_MEM);
    msgsend(status->src, &resp, sizeof(Net_socket_resp_t));
    return;
  }
  /* Create lwIP connection pcb based on socket type */
  switch(req->stype) {
  case SOCK_STREAM:
    err = bsock_new_tcp(sockp);
    break;
  case SOCK_RAW:
    err = bsock_new_raw(sockp, req->protocol);
    break;
  case SOCK_DGRAM:
    err = bsock_new_udp(sockp);
    break;
  default:
    err = ERR_VAL;
    break;
  }
  /* Had an error */
  if (err != ERR_OK) {
    resp.sockfd = -1;
    resp.errno = err_to_errno(err);
    bsock_free_sockfd(i);
  } else {
    /* No error */
    resp.sockfd = i;
    resp.errno = err_to_errno(ERR_OK);
  }
  msgsend(status->src, &resp, sizeof(Net_socket_resp_t));
}

void bsock_bind(Net_bind_req_t *req, Msg_status_t *status) {
  Net_bind_resp_t resp;
  bsock_t *sock;
  struct sockaddr *name;
  struct sockaddr_in *name_in;
  socklen_t namelen;
  ip_addr_t local_addr;
  uint16_t local_port;
  err_t err;
  int ret;
  
  name    = (struct sockaddr *)   (&(req->my_addr));
  name_in = (struct sockaddr_in *)(&(req->my_addr));
  namelen = req->addrlen;
  
  resp.type = NET_BIND;
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    ret = -1;
    err = ERR_CONN;
    goto bsock_bind_out;
  }
  
  /* Check size, family and alignment of 'name' */
  if ((namelen != sizeof(struct sockaddr_in)) || (name->sa_family != AF_INET)
      || (((uint64_t)(name) % 4) != 0)) {
    ret = -1;
    err = ERR_VAL;
    goto bsock_bind_out;
  }
  
  inet_addr_to_ipaddr(&local_addr, &name_in->sin_addr);
  local_port = ntohs(name_in->sin_port);
  
  err = ERR_VAL;
  if (sock->conn != NULL) {
    if (ERR_IS_FATAL(sock->last_err)) {
      err = sock->last_err; /* FIXME CONVERT TO SOCKET ERROR */
    } else {
      switch (sock->stype) {
	
      case NETCONN_RAW:
	err = raw_bind(raw_conn(sock), &local_addr);
	break;
	
      case NETCONN_UDP:
	err = udp_bind(udp_conn(sock), &local_addr, local_port);
	break;
	
      case NETCONN_TCP:
	err = tcp_bind(tcp_conn(sock), &local_addr, local_port);
	break;
	
      default:
	break;
	
      }
    }
  }
  
  if (err == ERR_OK)
    ret = 0;
  else
    ret = -1;
  
 bsock_bind_out:
  resp.ret = ret;
  resp.errno = err_to_errno(err);
  msgsend(status->src, &resp, sizeof(Net_bind_resp_t));
  return;
}

void bsock_listen(Net_listen_req_t *req, Msg_status_t *status) {
  Net_listen_resp_t resp;
  uint8_t backlog;
  bsock_t *sock;
  err_t err;
  int ret;
  
  BSOCK_FUNC_ENTER();
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    ret = -1;
    err = ERR_CONN;
    goto bsock_listen_out;
  }	
  
  /* Limit the "backlog" parameter to fit in a uint8_t */
  backlog = LWIP_MIN(LWIP_MAX((req->backlog), 0), 0xff);
  
  if (ERR_IS_FATAL(sock->last_err)) {
    err = sock->last_err; /* FIXME CONVERT TO SOCKET ERROR */
  } else {
    err = ERR_CONN;
    if (sock->conn != NULL) {
      if (sock->stype == NETCONN_TCP) {
	if (sock->state == NETCONN_NONE) {
	  struct tcp_pcb *lpcb = tcp_listen_with_backlog(tcp_conn(sock), backlog);
	  if (lpcb == NULL) {
	    BSOCK_LOG("bsock_listen(): lpcb alloc failed\n");
	    err = ERR_MEM;
	  } else {
	    /* Try to make this a listening sock. */
	    bsock_free_rq(sock); /* Don't need this anymore... */
	    sock->aq = qopen();
	    if (sock->aq != NULL) {
	      sock->aq_valid = 1;
	      sock->aq_len = 0;
	      err = ERR_OK;
	    } else {
	      BSOCK_LOG("bosck_listen(): could not open aq\n");
	      err = ERR_MEM;
	    }
	    
	    if (err == ERR_OK) {
	      sock->state = NETCONN_LISTEN;
	      sock->conn = (void*)lpcb;
	      tcp_arg(tcp_conn(sock), sock);
	      tcp_accept(tcp_conn(sock), bsock_accept_function);
	    } else {
	      /* since the old pcb is already deallocated, free lpcb now */
	      BSOCK_LOG("Giving up on lpcb\n");
	      tcp_close(lpcb);
	      sock->conn = NULL;
	    }
	  }
	}
      }
    }
  }
  
  if (err == ERR_OK)
    ret = 0;
  else
    ret = -1;
  
 bsock_listen_out:
  resp.type = NET_LISTEN;
  resp.ret = ret;
  resp.errno = err_to_errno(err);
  msgsend(status->src, &resp, sizeof(Net_listen_resp_t));
}

void bsock_connect(Net_connect_req_t *req, Msg_status_t *status) {
  bsock_t *sock;
  struct sockaddr *name;
  struct sockaddr_in *name_in;
  socklen_t namelen;
  err_t err;
  
  name    = (struct sockaddr *)   (&(req->serv_addr));
  name_in = (struct sockaddr_in *)(&(req->serv_addr));
  namelen = req->addrlen;
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_connect_out;
  }
  /* Check size, family and alignment of name */
  if ((namelen != sizeof(struct sockaddr_in)) || (name->sa_family != AF_INET)
      || (((uint64_t)(name) % 4) != 0)) {
    err = ERR_VAL;
    goto bsock_connect_out;
  }
  
  if (name_in->sin_family == AF_UNSPEC) {
    /* err = netconn_disconnect(sock->conn); */ /* FIXME */
    err = ERR_VAL;
  } else {
    ip_addr_t remote_addr;
    u16_t remote_port;
    
    inet_addr_to_ipaddr(&remote_addr, &name_in->sin_addr);
    remote_port = name_in->sin_port;
    
    /* Time to become a synner */
    err = bsock_do_connect(sock, &remote_addr, ntohs(remote_port));
  }
  
  
 bsock_connect_out:
  /* If we errored out, respond. */
  if (err != ERR_OK) {
    Net_connect_resp_t resp;
    resp.type  = NET_CONNECT;
    resp.ret   = -1;
    resp.errno = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_connect_resp_t));
  } else {
    /* Caller will get a response once connected. */
    sock->blocked_on = BSOCK_BL_CONNECT;
  }
}


void bsock_accept(Net_accept_req_t *req, Msg_status_t *status) {
  bsock_t *sock;
  err_t err;
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    BSOCK_LOG("bsock_accept(): Invalid sock\n");
    err = ERR_CONN;
    goto bsock_accept_out;
  }
  
  if (sock->stype != NETCONN_TCP) {
    err = ERR_ABRT;
    goto bsock_accept_out;
  }
  
  if (bsock_is_nonblocking(sock) && (sock->aq_len <= 0)) {
    BSOCK_LOG("bsock_accept(): ewouldblock\n");
    err = ERR_WOULDBLOCK;
    goto bsock_accept_out;
  }
  
  sock->copy_addr = req->copy_addr;
  sock->addrlen   = req->addrlen;
  err = bsock_do_accept(sock);
  
  
 bsock_accept_out:
  /* If there was an error, respond now. */
  if (err != ERR_OK) {
    Net_accept_resp_t resp;
    
    BSOCK_LOG("[BSOCK] Error during accept\n");
    resp.type = NET_ACCEPT;
    resp.ret  = -1;
    resp.errno = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_accept_resp_t));
  }
  
  /* User will be woken up upon accept through bsock_wakeup_accept(). */
}

void bsock_send(Net_send_req_t *req, Msg_status_t *status) {
  Net_send_resp_t resp;
  bsock_t *sock;
  int flags;
  uint8_t write_flags;
  err_t err;
  size_t size;
  
  flags = req->hdr.flags;
  size = req->hdr.len;
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->hdr.sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_send_out;
  }
  
  /* Valid length? */
  if (size > SOCKET_SNDBUF_MAX) {
    BSOCK_LOG("[BSOCK] Error: too big a buffer given to bsock_send\n");
    err = ERR_VAL;
    goto bsock_send_out;
  }
  
  /* Send the data */
  if (sock->stype != NETCONN_TCP) {
    /* TODO */
    BSOCK_LOG("[BSOCK] Error: sendto() not implemented\n");
    err = -1;
    /*		err = lwip_sendto(s, data, size, flags, NULL, 0); */
    goto bsock_send_out;
  }
  
  if ((flags & MSG_DONTWAIT) || bsock_is_nonblocking(sock)) {
    if ((size > TCP_SND_BUF) || ((size / TCP_MSS) > TCP_SND_QUEUELEN)) {
      /* too much data to ever send nonblocking! */
      err = ERR_MEM;
      goto bsock_send_out;
    }
  }
  
  write_flags = NETCONN_COPY |
    ((flags & MSG_MORE)     ? NETCONN_MORE      : 0) |
    ((flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0);
  
  err = bsock_do_write(sock, req->buf, req->hdr.len, write_flags);
  
 bsock_send_out:
  
  /* If there was an error, inform user */
  if (err != ERR_OK) {
    resp.type = NET_SEND;
    resp.ret = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_send_resp_t));
  }
  
  /* User will be notified when send finishes (or fails) */
}

void bsock_sendto(Net_send_req_t *req, Msg_status_t *status) {
  bsock_t *sock;
  Net_send_resp_t resp;
  err_t err;
  struct netbuf buf;
  struct sockaddr_in *to_in;
  uint16_t remote_port;
  uint16_t short_size;
  
  BSOCK_FUNC_ENTER();
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->hdr.sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_sendto_out;
  }
  
  /* Have we suffered a fatal error? */
  if (ERR_IS_FATAL(sock->last_err)) {
    err = sock->last_err;
    goto bsock_sendto_out;
  }
  
  /* Is this a valid sized message? */
  short_size = (uint16_t)(req->hdr.len);
  if (short_size > 1450) { /* FIXME this was arbitrarily chosen */
    err = ERR_ARG;
    goto bsock_sendto_out;
  }
  
  /* Check for alignment (sanity check) */
  if ((((mem_ptr_t)(&(req->hdr.dst))) % 4) != 0) {
    err = ERR_ARG;
    goto bsock_sendto_out;
  }
  
  to_in = (struct sockaddr_in *)(&(req->hdr.dst));
  
  /* initialize a buffer */
  buf.p = buf.ptr = NULL;
  if (req->hdr.addr_valid) {
    inet_addr_to_ipaddr(&buf.addr, &to_in->sin_addr);
    remote_port           = ntohs(to_in->sin_port);
    netbuf_fromport(&buf) = remote_port;
  } else {
    remote_port           = 0;
    ip_addr_set_any(&buf.addr);
    netbuf_fromport(&buf) = 0;
  }
  
  /* make the buffer point to the data that should be sent */
  err = netbuf_ref(&buf, req->buf, short_size);
  if (err != ERR_OK) {
    goto bsock_sendto_out;
  }
  
  err = bsock_do_sendto(sock, &buf);
  
  /* deallocated the buffer */
  netbuf_free(&buf);
  
 bsock_sendto_out:
  
  /* Send response */
  resp.type = NET_SENDTO;
  resp.ret = (err == ERR_OK) ? short_size : -1;
  resp.errno = err_to_errno(err);
  msgsend(status->src, &resp, sizeof(Net_recv_hdr_t));
  
  BSOCK_FUNC_EXIT();
}

void bsock_recvfrom(Net_recv_req_t *req, Msg_status_t *status) {
  bsock_t *sock;
  err_t err;
  
  BSOCK_FUNC_ENTER();
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_recvfrom_out;
  }
  
  /* Have we suffered a fatal error? */
  if (ERR_IS_FATAL(sock->last_err)) {
    err = sock->last_err;
    goto bsock_recvfrom_out;
  }
  
  /* Do the recvfrom part */
  err = bsock_do_recvfrom(sock, req->len, req->flags);
  
 bsock_recvfrom_out:
  /* Send response immediately if there was an error */
  if (err != ERR_OK) {
    Net_recv_hdr_t resp;
    BSOCK_LOG("[BSOCK] bsock_recvfrom errored out\n");
    resp.type = NET_RECVFROM;
    resp.sockfd = req->sockfd;
    resp.ret = -1;
    resp.errno = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_recv_hdr_t));
  }
  
  /* Otherwise, a message will be sent once recv completes */
  BSOCK_FUNC_EXIT();
}

void bsock_recv(Net_recv_req_t *req, Msg_status_t *status) {
  bsock_t *sock;
  err_t err;
  
  BSOCK_FUNC_ENTER();
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    printf("invalid sockfd\n");
    goto bsock_recv_out;
  }
  
  /* Have we suffered a fatal error? */
  if (ERR_IS_FATAL(sock->last_err)) {
    err = sock->last_err;
    printf("fatal error: %d\n",(int)err);
    goto bsock_recv_out;
  }
  
  err = bsock_do_recvfrom(sock, req->len, req->flags);
  
 bsock_recv_out:
  
  /* Send response immediately if there was an error */
  if (err != ERR_OK) {
    Net_recv_hdr_t resp;
    BSOCK_LOG("[BSOCK] do_recvfrom errored out\n");
    resp.type = NET_RECV;
    resp.sockfd = req->sockfd;
    resp.ret = -1;
    resp.errno = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_recv_hdr_t));
  }
  
  /* Otherwise, a message will be sent once recv completes */
  BSOCK_FUNC_EXIT();
}


void bsock_ghbn(Net_ghbn_req_t *req, Msg_status_t *status) {
  
}

void bsock_close(Net_close_req_t *req, Msg_status_t *status) {
  Net_close_resp_t resp;
  bsock_t *sock;
  err_t err;
  int send_resp_now;
  
  /* Valid sockfd? */
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_close_out;
  }
  
  /* 
   * TCP sockets may need to block in order to send out FINs, ACK data, etc.
   *  So if this is a TCP socket && err == ERR_OK,
   *  that means we shouldn't send a message back right now.  bsock_wakeup() will
   *  be called when the connection has been fully closed.
   */
  send_resp_now = (sock->stype != NETCONN_TCP);
  err = bsock_do_delconn(sock);
  
 bsock_close_out:
  
  if (send_resp_now || err != ERR_OK) {
    resp.type = NET_CLOSE;
    resp.ret = err == ERR_OK ? 0 : -1;
    resp.errno = err_to_errno(err);
    msgsend(status->src, &resp, sizeof(Net_close_resp_t));
  }
  
  /* Otherwise, the caller will be notified when the TCP conn is fully closed */
}


void bsock_update_owner(Net_update_owner_req_t *req, Msg_status_t *status) {
  Net_update_owner_resp_t resp;
  bsock_t *sock;
  err_t err;
  
  sock = bsock_get_sock(req->sockfd);
  if (bsock_validate_sock(sock, status->src) != 0) {
    err = ERR_CONN;
    goto bsock_update_ownder_out;
  }
  
  /*update the ownder of the socket */
  BSOCK_LOG("[BSOCK] Transferring socket pid owner!");
  sock->pid = req->pid;
  err = ERR_OK;
  
 bsock_update_ownder_out:
  
  resp.type = NET_UPDATE; 
  resp.ret = err == ERR_OK ? 0 : -1;
  resp.errno = err_to_errno(err);
  msgsend(status->src, &resp, sizeof(Net_update_owner_resp_t));
}

int bsock_has_data(int sockfd) {
  bsock_t *sock = bsock_get_sock(sockfd);
  if(sock == NULL) return 0;
  return (sock->rq_len > 0);
}

int bsock_is_closed(int sockfd) {
  bsock_t *sock = bsock_get_sock(sockfd);
  return 0;
  if(sock == NULL) return 1;
  else return (sock->shutdown_state == NETCONN_SHUT_RDWR || sock->state == NETCONN_CLOSE);
}

/*** PRIVATE HELPER FUNCTIONS ***/

static int bsock_validate_sock(bsock_t *sock, pid_t pid) {
  int rc;
  
  rc = 0;
  
  /* Valid sockfd? */
  if (sock == NULL) {
    rc = -1;
  } else if (sock->pid != pid) {
    BSOCK_LOG("[BSOCK] Found spoofed message?!");
    rc = ERR_CONN;
  }
  
  return rc;
}

/* Allocate socket structure */
static int bsock_alloc_sockfd(void *conn, int accepted, int pid, bsock_t **sockp) {
  bsock_t *sp;
  
  if(!is_sockid(nextsockfd))
    return ERR_MEM;
  if((sp = (bsock_t*)malloc(sizeof(bsock_t)))==NULL) {
    *sockp = NULL;
    return ERR_MEM;
  }
  sp->sockfd       = nextsockfd;			
  sp->active       = 1;
  sp->conn         = conn;
  sp->pid          = pid;
  sp->last_err     = ERR_OK;
  sp->state        = NETCONN_NONE;
  if(accepted)
    sp->blocked_on   = BSOCK_BL_ACCEPT;
  else
    sp->blocked_on   = BSOCK_BL_NONE;

  sp->write_offset = 0;
  sp->write_flags  = 0;
  sp->write_len    = 0;
  sp->write_buf    = NULL;
  
  sp->aq           = NULL;	/* clients: no accept queue */
  sp->aq_valid     = 0;		/* accept queue is not valid */
  sp->rq           = qopen();	/* request queue is open */
  sp->rq_valid     = ((sp->rq) != NULL); /* and valid */
  sp->recv_len     = 0;
  sp->rq_len       = 0;
  sp->last_off     = 0;
  sp->callback     = bsock_event_callback;
  shput(opensockets,sp,nextsockfd);
  *sockp=sp;
  return nextsockfd++;
}

/* Free socket structure.  conn must be already freed! */
static void bsock_free_sockfd(int sockfd) {
  bsock_t *sockp;
  sockp = shremove(opensockets,lookup,sockfd);
  free(sockp);
}


static bsock_t *bsock_get_sock(int sockfd) {
  bsock_t *sockp;
  
  if((sockp = shsearch(opensockets, lookup, sockfd))==NULL) {
    BSOCK_LOG("[bsock] invalid sockfd\n");
    return NULL;
  }
  if (sockp->active == 0) {
    BSOCK_LOG("[bosck] request on inactive sockfd\n");
    return NULL;
  }
  return sockp;
}

static err_t bsock_new_tcp(bsock_t *sock) {
  sock->conn = (void*)tcp_new();
  if (sock->conn == NULL) {
    BSOCK_LOG("[bsock] tcp_new() failed\n");
    return ERR_MEM;
  }
  sock->stype = NETCONN_TCP;
  bsock_setup_tcp(sock);
  return ERR_OK;
}

static err_t bsock_new_udp(bsock_t *sock) {
  sock->conn = (void*)udp_new();
  if(sock->conn == NULL) {
    BSOCK_LOG("[bsock udp_new() failed\n");
    return ERR_MEM;
  }
  sock->stype = NETCONN_UDP;
  udp_recv(udp_conn(sock), bsock_recv_udp, sock);
  return ERR_OK;
}

static err_t bsock_new_raw(bsock_t *sock, int protocol) {
  sock->conn = (void*)raw_new(protocol);
  if(sock->conn == NULL) {
    BSOCK_LOG("[bsock] raw_new() failed\n");
    return ERR_MEM;
  }
  sock->stype = NETCONN_RAW;
  raw_recv(raw_conn(sock), bsock_recv_raw, sock);
  return ERR_OK;
}

/**
 * Connect a pcb contained inside a netconn
 * Called from netconn_connect.
 *
 * @param msg the api_msg_msg pointing to the connection and containing
 *            the IP address and port to connect to
 */
static err_t bsock_do_connect(bsock_t *sock, ip_addr_t *ipaddr, uint16_t port) {
  err_t err;
  
  if (sock->conn == NULL) {
    /* This may happen when calling netconn_connect() a second time */
    err = ERR_CLSD;
  } else {
    switch (sock->stype) {
      
    case NETCONN_RAW:
      err = raw_connect(raw_conn(sock), ipaddr);
      break;
      
    case NETCONN_UDP:
      err = udp_connect(udp_conn(sock), ipaddr, port);
      break;
      
    case NETCONN_TCP:
      /* Prevent connect while doing any other action. */
      if (sock->state != NETCONN_NONE) {
	err = ERR_ISCONN;
      } else {
	bsock_setup_tcp(sock);
	err = tcp_connect(tcp_conn(sock), ipaddr, port, bsock_do_connected);
	
	if (err == ERR_OK) {
	  u8_t non_blocking = bsock_is_nonblocking(sock);
	  sock->state = NETCONN_CONNECT;
	  SET_NONBLOCKING_CONNECT(sock, non_blocking);
	  if (non_blocking) {
	    err = ERR_INPROGRESS;
	  } else {
	    /* FIXME: WHAT DO I NEED TO SAVE IN THE STRUCT? */
	    /* conn->current_msg = msg; */
	    err = ERR_OK;
	  }
	}
      }
      break;
      
    default:
      BSOCK_LOG("[BSOCK] Error: Invalid socket type\n");
      break;
    }
  }
  
  return err;
}

/**
 * TCP callback function if a connection (opened by tcp_connect/do_connect) has
 * been established (or reset by the remote host).
 *
 * @see tcp.h (struct tcp_pcb.connected) for parameters and return values
 */
static err_t bsock_do_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
  bsock_t *sock;
  int was_blocking;
  
  LWIP_UNUSED_ARG(pcb);
  
  BSOCK_FUNC_ENTER();
  
  sock = (bsock_t *)arg;
  
  if (sock == NULL) {
    return ERR_VAL;
  }
  
  if ((sock->stype == NETCONN_TCP) && (err == ERR_OK)) {
    bsock_setup_tcp(sock);
  }
  was_blocking = !IN_NONBLOCKING_CONNECT(sock);
  SET_NONBLOCKING_CONNECT(sock, 0);
  sock->state = NETCONN_NONE;
  if (!was_blocking) {
    NETCONN_SET_SAFE_ERR(sock, ERR_OK);
  }
  API_EVENT(sock, NETCONN_EVT_SENDPLUS, 0);
  
  /* If this was a blocking connect, wake up the process. */
  if (was_blocking) {
    bsock_wakeup(sock,err);
  }
  
  return ERR_OK;
}


static err_t bsock_do_accept(bsock_t *sock) {
  err_t err;
  
  BSOCK_FUNC_ENTER();
  
  /* 
   * don't recv on fatal errors: this might block the application task
   * waiting on acceptmbox forever! 
   */
  err = sock->last_err;
  if (ERR_IS_FATAL(err)) {
    BSOCK_LOG("[BSOCK] bsock_do_accept(): fatal error encountered\n");
    return err;
  }
  
  
  if (sock->aq_len > 0) {
    /* There's a connection waiting, wake up user now. */
    //		BSOCK_LOG("[BSOCK] bsock_do_accept(): connection waiting\n");
    bsock_wakeup_accept(sock, ERR_OK);
  } else {
    /* No connection waiting.  Block until one arrives */
    //		BSOCK_LOG("[BSOCK] bsock_do_accept(): blocking til incoming connect\n");
    sock->blocked_on = BSOCK_BL_ACCEPT;
  }
  
  return ERR_OK;
}

/**
 * Accept callback function for TCP netconns.
 * Allocates a new netconn and posts that to conn->acceptmbox.
 *
 * @see tcp.h (struct tcp_pcb_listen.accept) for parameters and return value
 */
static err_t bsock_accept_function(void *arg, struct tcp_pcb *newpcb, err_t err) {
  bsock_t *sockp;
  bsock_t *newsockp;
  int newsockfd;
  
  BSOCK_FUNC_ENTER();
  
  sockp = (bsock_t *)arg;
  if (sockp->aq_valid == 0) {
    BSOCK_LOG("[BSOCK] accept_function: aq invalid\n");
    return ERR_VAL;
  }
  
  /* Allocate a new sockfd */
  if((newsockfd = bsock_alloc_sockfd(newpcb, 1, sockp->pid, &newsockp))==ERR_MEM) 
    return ERR_MEM; 
  /* the newsocket is blocking by default (??-do we really need this??) */
  bsock_set_nonblocking(newsockp,0);
  newsockp->stype = sockp->stype;
  bsock_setup_tcp(newsockp);
  /* 
   * no protection: when creating the pcb, the netconn is not yet
   * known to the application thread
   */
  newsockp->last_err = err;
  if (qput(sockp->aq, newsockp) != 0) {
    /* FIXME: Is this right? What about api events in delconn, that ok? */
    bsock_do_delconn(newsockp);
    bsock_free_sockfd(newsockp->sockfd);
    return ERR_MEM;
  }
  else {
    sockp->aq_len += 1;
    API_EVENT(sockp, NETCONN_EVT_RCVPLUS, 0);
  }
  /* If the sock is blocked on an accept, wake it up. */
  bsock_wakeup(sockp, err);
  return ERR_OK;
}

static err_t bsock_do_sendto(bsock_t *sock, struct netbuf *buf) {
  err_t err;
  
  err = ERR_CONN;
  if (sock->conn != NULL) {
    switch (NETCONNTYPE_GROUP(sock->stype)) {
    case NETCONN_RAW:
      if (ip_addr_isany(&(buf->addr))) {
	err = raw_send(raw_conn(sock), buf->p);
      } else {
	err = raw_sendto(raw_conn(sock), buf->p, &buf->addr);
      }
      break;
    case NETCONN_UDP:
      if (ip_addr_isany(&(buf->addr))) {
	err = udp_send(udp_conn(sock), buf->p);
      } else {
	err = udp_sendto(udp_conn(sock), buf->p, &(buf->addr), buf->port);
      }
      break;
    default:
      break;
    }
  }
  
  return err;
}

/**
 * Send some data on a TCP pcb contained in a netconn
 * Called from netconn_write
 *
 * @param msg the api_msg_msg pointing to the connection
 */
static err_t bsock_do_write(bsock_t *sock, void *dataptr, size_t size, uint8_t apiflags) {
  err_t err;
  void *buf;
  
  if (ERR_IS_FATAL(sock->last_err)) {
    err = sock->last_err;
  } else {
    if (sock->stype == NETCONN_TCP) {
      
      if (sock->state != NETCONN_NONE) {
	/* netconn is connecting, closing or in blocking write */
	err = ERR_INPROGRESS;
      } else if (tcp_conn(sock) != NULL) {
	buf = malloc(size);
	if (buf == NULL) {
	  err = ERR_MEM;
	} else {
	  memcpy(buf,dataptr,size);
	  sock->state = NETCONN_WRITE;
	  /* set all the variables used by do_writemore */
	  sock->write_offset = 0;
	  sock->write_flags  = apiflags;
	  sock->write_buf    = buf;
	  sock->write_len    = size;
	  sock->blocked_on   = BSOCK_BL_SEND;
	  
	  err = bsock_do_writemore(sock);
	  if (err != ERR_OK) {
	    sock->blocked_on = BSOCK_BL_NONE;
	    free(buf);
	  }
	  /* for both cases: if do_writemore was called, don't ACK the APIMSG
	     since do_writemore ACKs it! */
	}
      } else {
	err = ERR_CONN;
      }
    } else {
      err = ERR_VAL;
    }
  }
  
  return err;
}


/**
 * See if more data needs to be written from a previous call to netconn_write.
 * Called initially from do_write. If the first call can't send all data
 * (because of low memory or empty send-buffer), this function is called again
 * from sent_tcp() or poll_tcp() to send more data. If all data is sent, the
 * blocking application thread (waiting in netconn_write) is released.
 *
 * @param conn netconn (that is currently in state NETCONN_WRITE) to process
 * @return ERR_OK
 *         ERR_MEM if LWIP_TCPIP_CORE_LOCKING=1 and sending hasn't yet finished
 */
static err_t bsock_do_writemore(bsock_t *sock) {
  err_t err = ERR_OK;
  void *dataptr;
  u16_t len, available;
  u8_t write_finished = 0;
  size_t diff;
  u8_t dontblock = bsock_is_nonblocking(sock) ||
    (sock->write_flags & NETCONN_DONTBLOCK);
  u8_t apiflags = sock->write_flags;
  
  
  dataptr = (u8_t*)(sock->write_buf) + sock->write_offset;
  diff = sock->write_len - sock->write_offset;
  if (diff > 0xffffUL) { /* max_u16_t */
    len = 0xffff;
    apiflags |= TCP_WRITE_FLAG_MORE;
  } else {
    len = (u16_t)diff;
  }
  available = tcp_sndbuf(tcp_conn(sock));
  if (available < len) {
    /* don't try to write more than sendbuf */
    len = available;
    apiflags |= TCP_WRITE_FLAG_MORE;
  }
  if (dontblock && (len < sock->write_len)) {
    /* failed to send all data at once -> nonblocking write not possible */
    err = ERR_MEM;
  }
  if (err == ERR_OK) {
    err = tcp_write(tcp_conn(sock), dataptr, len, apiflags);
  }
  if (dontblock && (err == ERR_MEM)) {
    /* nonblocking write failed */
    write_finished = 1;
    err = ERR_WOULDBLOCK;
    /* let poll_tcp check writable space to mark the pcb
       writable again */
    sock->flags |= NETCONN_FLAG_CHECK_WRITESPACE;
    /* let select mark this pcb as non-writable. */
    API_EVENT(sock, NETCONN_EVT_SENDMINUS, len);
  } else {
    /* if OK or memory error, check available space */
    if (((err == ERR_OK) || (err == ERR_MEM)) &&
	((tcp_sndbuf(tcp_conn(sock)) <= TCP_SNDLOWAT) ||
	 (tcp_sndqueuelen(tcp_conn(sock)) >= TCP_SNDQUEUELOWAT))) {
      /* The queued byte- or pbuf-count exceeds the configured low-water limit,
	 let select mark this pcb as non-writable. */
      API_EVENT(sock, NETCONN_EVT_SENDMINUS, len);
    }
    
    if (err == ERR_OK) {
      sock->write_offset += len;
      if (sock->write_offset == sock->write_len) {
	/* everything was written */
	write_finished = 1;
	sock->write_offset = 0;
      }
      tcp_output(tcp_conn(sock));
    } else if (err == ERR_MEM) {
      /* If ERR_MEM, we wait for sent_tcp or poll_tcp to be called
	 we do NOT return to the application thread, since ERR_MEM is
	 only a temporary error! */
      
      /* tcp_write returned ERR_MEM, try tcp_output anyway */
      tcp_output(tcp_conn(sock));
      
    } else {
      /* On errors != ERR_MEM, we don't try writing any more but return
	 the error to the application thread. */
      write_finished = 1;
    }
  }
  
  if (write_finished) {
    /* everything was written: set back connection state
       and back to application task */
    sock->state = NETCONN_NONE;
    bsock_wakeup(sock,err);
  }
  return ERR_OK;
}

static err_t bsock_do_recvfrom(bsock_t *sock, size_t len, int flags) {
  
  BSOCK_FUNC_ENTER();
  
  /* Set these so we can call bsock_wakeup() later. */
  BSOCK_LOG("[BSOCK] blocking sockfd %d on bsock_bl_recv\n",sock->sockfd);
  sock->blocked_on = BSOCK_BL_RECV;
  
  sock->recv_flags = flags;
  
  /* Due to message-passing constraints, a recv can only pass back so much info */
  if (len > SOCKET_SNDBUF_MAX)
    len = SOCKET_SNDBUF_MAX;
  
  sock->recv_len = len;
  
  
  /* Check if there is enough data left from last recv operation */
  if (((sock->stype == NETCONN_TCP) && ((sock->rq_len) >= len)) ||
      ((sock->stype == NETCONN_UDP) && ((sock->rq_len) > 0))) {
    BSOCK_LOG("[BSOCK] Enough data left from previous op\n");
    bsock_wakeup(sock, ERR_OK);
    return ERR_OK;
  }
  
  /* If this is nonblocking, return what is there or EWOULDBLOCK. */
  if ((flags & MSG_DONTWAIT) || bsock_is_nonblocking(sock)) {
    if (sock->rq_len > 0) {
      BSOCK_LOG("[BSOCK] Nonblocking; some data ready\n");
      bsock_wakeup(sock, ERR_OK);
      return ERR_OK;
    } else {
      BSOCK_LOG("[BSOCK] Nonblocking; ewouldblock\n");
      return ERR_WOULDBLOCK;
    }
  }
  
  /* This call is gonna block. Everything else happens later upon recv. */
  BSOCK_LOG("[BSOCK] Regular blocking call.\n");
  return ERR_OK;
}


/**
 * Receive callback function for TCP netconns.
 * Posts the packet to conn->recvmbox, but doesn't delete it on errors.
 *
 * @see tcp.h (struct tcp_pcb.recv) for parameters and return value
 */
static err_t bsock_recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
  bsock_t *sock;
  u16_t len;
  
  BSOCK_FUNC_ENTER();
  
  LWIP_UNUSED_ARG(pcb);
  sock = (bsock_t *)arg;
  
  if (sock == NULL) {
    BSOCK_LOG("recv_tcp: sock NULL\n");
    return ERR_VAL;
  }
  
  if ((sock->rq_valid) == 0) {
    /* recv queue already deleted */
    if (p != NULL) {
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
    }
    return ERR_OK;
  }
  
  /* Unlike for UDP or RAW pcbs, don't check for available space
     using recv_avail since that could break the connection
     (data is already ACKed) */
  
  /* don't overwrite fatal errors! */
  NETCONN_SET_SAFE_ERR(sock, err);
  
  if (p != NULL) {
    len = p->tot_len;
  } else {
    len = 0;
  }
  
  if (qput(sock->rq, p) != 0) {
    /* don't deallocate p: it is presented to us later again from tcp_fasttmr! */
    BSOCK_LOG("recv_tcp: qput failed\n");
    return ERR_MEM;
  } else {
    /* Register event with callback */
    API_EVENT(sock, NETCONN_EVT_RCVPLUS, len);
    
    /* Update queue len */
    sock->rq_len += len;
    
    /* Time to wake up the caller? */
    if ((sock->blocked_on) == BSOCK_BL_RECV) {
      BSOCK_LOG("Blocked on recv... ");
      if ((p == NULL) || 
	  ((sock->rq_len) >= (sock->recv_len)) ||
	  ((sock->rq_len) >= SOCKET_SNDBUF_MAX)) {
	BSOCK_LOG("Tiem to wake up!");
	bsock_wakeup(sock, ERR_OK);
      }
      BSOCK_LOG("\n");
    } else {
      BSOCK_LOG("Sockfd %d Not blocked on recv\n", sock->sockfd);
    }
  }
  
  return ERR_OK;
}

/**
 * Receive callback function for UDP netconns.
 * Posts the packet to conn->recvmbox or deletes it on memory error.
 *
 * @see udp.h (struct udp_pcb.recv) for parameters
 */
static void bsock_recv_udp(void *arg, struct udp_pcb *pcb, struct pbuf *p,
			   ip_addr_t *addr, u16_t port) {
  bsock_t *sock;
  struct netbuf *buf;
  
  BSOCK_FUNC_ENTER();
  
  sock = (bsock_t *)arg;
  
  /* sanity check arguments */
  if ((sock == NULL) || (sock->stype != NETCONN_UDP) || ((udp_conn(sock)) != pcb)) {
    BSOCK_LOG("recv_udp: invalid sock\n");
    pbuf_free(p);
    return;
  }
  
  /* Has this socket been closed */
  if (sock->rq_valid == 0) {
    BSOCK_LOG("recv_udp: socket is closed\n");
    pbuf_free(p);
    return;
  }
  
  /* 
   * Allocate a 'netbuf,' which holds pointers to the packet as well as
   * source IP/port.
   */
  buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
  if (buf == NULL) {
    BSOCK_LOG("Could not alloc netbuf\n");
    pbuf_free(p);
    return;
  } else {
    buf->p = p;
    buf->ptr = p;
    ip_addr_set(&buf->addr, addr);
    buf->port = port;
  }
  BSOCK_LOG("buf->p = 0x%x\n",buf->p);
  
  /* 
   * Add the packet to this socket's recv queue, and if its owning process is
   * waiting, wake it up.
   */
  if (qput(sock->rq, buf) != 0) {
    netbuf_delete(buf);
    return;
  } else {
    
    /* Update queue len */
    sock->rq_len += p->tot_len;
    
    /* Time to wake up the caller? */
    if ((sock->blocked_on) == BSOCK_BL_RECV)
      bsock_wakeup(sock, ERR_OK);
    
    /* Register event with callback */
    API_EVENT(sock, NETCONN_EVT_RCVPLUS, p->tot_len);
  }
}

/**
 * Receive callback function for RAW netconns.
 * Doesn't 'eat' the packet, only references it and sends it to
 * conn->recvmbox
 *
 * @see raw.h (struct raw_pcb.recv) for parameters and return value
 */
static u8_t bsock_recv_raw(void *arg, struct raw_pcb *pcb, struct pbuf *p,
			   ip_addr_t *addr) {
  BSOCK_FUNC_ENTER();
#if 0 /* TODO */
  struct pbuf *q;
  struct netbuf *buf;
  struct netconn *conn;
  
  LWIP_UNUSED_ARG(addr);
  conn = (struct netconn *)arg;
  
  if ((conn != NULL) && sys_mbox_valid(&conn->recvmbox)) {
#if LWIP_SO_RCVBUF
    int recv_avail;
    SYS_ARCH_GET(conn->recv_avail, recv_avail);
    if ((recv_avail + (int)(p->tot_len)) > conn->recv_bufsize) {
      return 0;
    }
#endif /* LWIP_SO_RCVBUF */
    /* copy the whole packet into new pbufs */
    q = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
    if(q != NULL) {
      if (pbuf_copy(q, p) != ERR_OK) {
        pbuf_free(q);
        q = NULL;
      }
    }
    
    if (q != NULL) {
      u16_t len;
      buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
      if (buf == NULL) {
        pbuf_free(q);
        return 0;
      }
      
      buf->p = q;
      buf->ptr = q;
      ip_addr_copy(buf->addr, *ip_current_src_addr());
      buf->port = pcb->protocol;
      
      len = q->tot_len;
      if (sys_mbox_trypost(&conn->recvmbox, buf) != ERR_OK) {
        netbuf_delete(buf);
        return 0;
      } else {
#if LWIP_SO_RCVBUF
        SYS_ARCH_INC(conn->recv_avail, len);
#endif /* LWIP_SO_RCVBUF */
        /* Register event with callback */
        API_EVENT(conn, NETCONN_EVT_RCVPLUS, len);
      }
    }
  }
  
#endif
  return 0; /* do not eat the packet */
}


/**
 * Poll callback function for TCP netconns.
 * Wakes up an application thread that waits for a connection to close
 * or data to be sent. The application thread then takes the
 * appropriate action to go on.
 *
 * Signals the conn->sem.
 * netconn_close waits for conn->sem if closing failed.
 *
 * @see tcp.h (struct tcp_pcb.poll) for parameters and return value
 */
static err_t bsock_poll_tcp(void *arg, struct tcp_pcb *pcb) {
  bsock_t *sock;
  
  sock = (bsock_t *)arg;
  
  if (sock->state == NETCONN_WRITE) {
    bsock_do_writemore(sock);
  } else if (sock->state == NETCONN_CLOSE) {
    bsock_close_internal(sock);
  }
  /* @todo: implement connect timeout here? */
  
  /* Did a nonblocking write fail before? Then check available write-space. */
  if (sock->flags & NETCONN_FLAG_CHECK_WRITESPACE) {
    /* If the queued byte- or pbuf-count drops below the configured low-water limit,
       let select mark this pcb as writable again. */
    if ((tcp_conn(sock) != NULL) && (tcp_sndbuf(tcp_conn(sock)) > TCP_SNDLOWAT) &&
	(tcp_sndqueuelen(tcp_conn(sock)) < TCP_SNDQUEUELOWAT)) {
      sock->flags &= ~NETCONN_FLAG_CHECK_WRITESPACE;
      API_EVENT(sock, NETCONN_EVT_SENDPLUS, 0);
    }
  }
  
  return ERR_OK;
}

/**
 * Sent callback function for TCP netconns.
 * Signals the conn->sem and calls API_EVENT.
 * netconn_write waits for conn->sem if send buffer is low.
 *
 * @see tcp.h (struct tcp_pcb.sent) for parameters and return value
 */
static err_t bsock_sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len) {
  bsock_t *sock;
  
  BSOCK_FUNC_ENTER();
  
  sock = (bsock_t *)arg;
  
  if (sock->state == NETCONN_WRITE) {
    bsock_do_writemore(sock);
  } else if (sock->state == NETCONN_CLOSE) {
    bsock_close_internal(sock);
  }
  
  if (sock) {
    /* If the queued byte- or pbuf-count drops below the configured low-water limit,
       let select mark this pcb as writable again. */
    if ((tcp_conn(sock) != NULL) && (tcp_sndbuf(tcp_conn(sock)) > TCP_SNDLOWAT) &&
	(tcp_sndqueuelen(tcp_conn(sock)) < TCP_SNDQUEUELOWAT)) {
      sock->flags &= ~NETCONN_FLAG_CHECK_WRITESPACE;
      API_EVENT(sock, NETCONN_EVT_SENDPLUS, len);
    }
  }
  
  BSOCK_FUNC_EXIT();
  return ERR_OK;
}

/**
 * Delete the pcb inside a netconn.
 * Called from netconn_delete.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
static err_t bsock_do_delconn(bsock_t *sock) {
  err_t err;
  
  /* @todo TCP: abort running write/connect? */
  if ((sock->state != NETCONN_NONE) &&
      (sock->state != NETCONN_LISTEN) &&
      (sock->state != NETCONN_CONNECT)) {
    err = ERR_INPROGRESS;
  } else {
    /* Drain and delete mboxes */
    bsock_drain(sock);
    
    if (sock->conn != NULL) {
      
      switch (NETCONNTYPE_GROUP(sock->stype)) {
	
      case NETCONN_RAW:
	raw_remove(raw_conn(sock));
	break;
	
      case NETCONN_UDP:
	(udp_conn(sock))->recv_arg = NULL;
	udp_remove(udp_conn(sock));
	break;
	
      case NETCONN_TCP:
	sock->state = NETCONN_CLOSE;
	sock->shutdown_state = NETCONN_SHUT_RDWR;
	bsock_close_internal(sock);
	/* API_EVENT is called inside do_close_internal, before releasing
	   the application thread, so we can return at this point! */
	return ERR_OK;
	
      default:
	break;
      }
      sock->conn = NULL;
    }
    
    /* @todo: this lets select make the socket readable and writable,
       which is wrong! errfd instead? */
    API_EVENT(sock, NETCONN_EVT_RCVPLUS, 0);
    API_EVENT(sock, NETCONN_EVT_SENDPLUS, 0);
    
    /* 
     * TCP netconns don't come here! 
     * UDP and RAW sockets can be freed right away...
     */
    bsock_free_sockfd(sock->sockfd);
    
    err = ERR_OK;
  }
  return err;
}

static void bsock_drain(bsock_t *sock) {
  
  /* Free recv queue and all queued packets */
  bsock_free_rq(sock);
  
  /* Free accept queue and all queued connections. */
  bsock_free_aq(sock);
}

static void bsock_free_rq(bsock_t *sock) {
  
  /* Free all queued packets */
  if (sock->rq_valid) {
    
    /* Free all data */
    if (sock->stype == NETCONN_TCP) {
      qapply2(sock->rq, sock, &bsock_free_rq_tcp);
    } else {
      qapply(sock->rq, &bsock_free_rq_udp); /* and raw */
    }
    
    /* Close queue */
    qclose(sock->rq);
    sock->rq = NULL;
    sock->rq_valid = 0;
  }
}

static void bsock_free_aq(bsock_t *sock) {
  //	BSOCK_LOG("[BSOCK] bsock_free_aq() TODO\n");
}

static void bsock_free_rq_tcp(void *arg, void *elementp) {
  bsock_t *sock;
  struct pbuf *p;
  
  sock = (bsock_t *)arg;
  
  if (elementp != NULL) {
    p = (struct pbuf *)elementp;
    if (tcp_conn(sock) != NULL) {
      tcp_recved(tcp_conn(sock), p->tot_len);
    }
    pbuf_free((struct pbuf *)elementp);
  }
}

static void bsock_free_rq_udp(void *elementp) {
  if (elementp != NULL) {
    netbuf_delete((struct netbuf *)elementp);
  }
}

/**
 * Internal helper function to close a TCP netconn: since this sometimes
 * doesn't work at the first attempt, this function is called from multiple
 * places.
 *
 * @param conn the TCP netconn to close
 */
static void bsock_close_internal(bsock_t *sock) {
  err_t err;
  u8_t shut, shut_rx, shut_tx, close;
  
  shut = sock->shutdown_state;
  shut_rx = shut & NETCONN_SHUT_RD;
  shut_tx = shut & NETCONN_SHUT_WR;
  /* shutting down both ends is the same as closing */
  close = shut == NETCONN_SHUT_RDWR;
  
  sock->blocked_on = BSOCK_BL_CLOSE;
  
  /* Set back some callback pointers */
  if (close) {
    tcp_arg(tcp_conn(sock), NULL);
  }
  if ((tcp_conn(sock))->state == LISTEN) {
    tcp_accept(tcp_conn(sock), NULL);
  } else {
    /* some callbacks have to be reset if tcp_close is not successful */
    if (shut_rx) {
      tcp_recv(tcp_conn(sock), NULL);
      tcp_accept(tcp_conn(sock), NULL);
    }
    if (shut_tx) {
      tcp_sent(tcp_conn(sock), NULL);
    }
    if (close) {
      tcp_poll(tcp_conn(sock), NULL, 4);
      tcp_err(tcp_conn(sock), NULL);
    }
  }
  /* Try to close the connection */
  if (shut == NETCONN_SHUT_RDWR) {
    err = tcp_close(tcp_conn(sock));
  } else {
    err = tcp_shutdown(tcp_conn(sock), shut & NETCONN_SHUT_RD, shut & NETCONN_SHUT_WR);
  }
  if (err == ERR_OK) {
    /* Closing succeeded */
    
    sock->state = NETCONN_NONE;
    
    /* Set back some callback pointers as conn is going away */
    sock->conn = NULL;
    
    /* Trigger select() in socket layer. Make sure everybody notices activity
       on the connection, error first! */
    if (close) {
      API_EVENT(sock, NETCONN_EVT_ERROR, 0);
    }
    if (shut_rx) {
      API_EVENT(sock, NETCONN_EVT_RCVPLUS, 0);
    }
    if (shut_tx) {
      API_EVENT(sock, NETCONN_EVT_SENDPLUS, 0);
    }
    
    /* Wake up caller */
    bsock_wakeup(sock, err);
    
  } else {
    /* Closing failed, restore some of the callbacks */
    tcp_sent(tcp_conn(sock), bsock_sent_tcp);
    tcp_poll(tcp_conn(sock), bsock_poll_tcp, 4);
    tcp_err(tcp_conn(sock), bsock_err_tcp);
    tcp_arg(tcp_conn(sock), sock);
    /* don't restore recv callback: we don't want to receive any more data */
  }
  /* If closing didn't succeed, we get called again either
     from poll_tcp or from sent_tcp */
}


/**
 * Error callback function for TCP netconns.
 * Signals conn->sem, posts to all conn mboxes and calls API_EVENT.
 * The application thread has then to decide what to do.
 *
 * @see tcp.h (struct tcp_pcb.err) for parameters
 */
static void bsock_err_tcp(void *arg, err_t err) {
  BSOCK_FUNC_ENTER();
  
  bsock_t *sock;
  enum netconn_state old_state;
  
  sock = (bsock_t *)arg;
  
  sock->conn = NULL;
  
  /* no check since this is always fatal! */
  sock->last_err = err;
  
  /* reset conn->state now before waking up other threads */
  old_state = sock->state;
  sock->state = NETCONN_NONE;
  
  /* Notify the user layer about a connection error. Used to signal
     select. */
  API_EVENT(sock, NETCONN_EVT_ERROR, 0);
  /* Try to release selects pending on 'read' or 'write', too.
     They will get an error if they actually try to read or write. */
  API_EVENT(sock, NETCONN_EVT_RCVPLUS, 0);
  API_EVENT(sock, NETCONN_EVT_SENDPLUS, 0);
  
  /* pass NULL-message to recv queue to signal an error */
  BSOCK_LOG("err_tcp on sockfd %d\n",sock->sockfd);
  if (sock->rq_valid) {
    qput(sock->rq, NULL);
  }
  /* pass NULL-message to accept queue to signal an error */
  if (sock->aq_valid) {
    qput(sock->aq, NULL);
  }
  
  
  /* TODO: Any call-specific stuff that should happen, like nonblocking below */
  switch(old_state) {
    
  case NETCONN_WRITE:
    break;
    
  case NETCONN_CLOSE:
    break;
    
  case NETCONN_CONNECT:
    SET_NONBLOCKING_CONNECT(sock, 0);
    break;
    
  default:
    break;
  }
  
  bsock_wakeup(sock, err);
  
  BSOCK_FUNC_EXIT();
}


/**
 * Setup a tcp_pcb with the correct callback function pointers
 * and their arguments.
 *
 * @param conn the TCP netconn to setup
 */
static void bsock_setup_tcp(bsock_t *sock) {
  struct tcp_pcb *pcb;
  
  pcb = tcp_conn(sock);
  tcp_arg(pcb, sock);
  tcp_recv(pcb, bsock_recv_tcp);
  tcp_sent(pcb, bsock_sent_tcp);
  tcp_poll(pcb, bsock_poll_tcp, 4);
  tcp_err(pcb, bsock_err_tcp);
}

static void bsock_wakeup(bsock_t *sock, err_t err) {
  
  BSOCK_FUNC_ENTER();
  
  switch(sock->blocked_on) {
  case BSOCK_BL_NONE:
    BSOCK_LOG("Error: bsock_wakeup called with state = netconn_none\n");
    break;
    
  case BSOCK_BL_SOCKET:
  case BSOCK_BL_BIND:
    BSOCK_LOG("Error: bsock_wakeup called and blocked_on is a nonblocking func\n");
    break;
    
  case BSOCK_BL_LISTEN:
    bsock_wakeup_listen(sock,err);
    break;
    
  case BSOCK_BL_CONNECT:
    bsock_wakeup_connect(sock,err);
    break;
    
  case BSOCK_BL_ACCEPT:
    bsock_wakeup_accept(sock,err);
    break;
    
  case BSOCK_BL_SEND:
    bsock_wakeup_send(sock,err);
    break;
    
  case BSOCK_BL_RECV:
    bsock_wakeup_recv(sock,err);
    break;
    
  case BSOCK_BL_GHBN:
    bsock_wakeup_ghbn(sock,err);
    break;
    
  case BSOCK_BL_CLOSE:
    bsock_wakeup_close(sock,err);
    break;
  }
  sock->blocked_on = BSOCK_BL_NONE;
}

static void bsock_wakeup_listen(bsock_t *sock, err_t err) {
  Net_listen_resp_t resp;
  resp.type = NET_LISTEN;
  if (err == ERR_OK) {
    resp.ret = 0;
  } else {
    resp.ret = -1;
  }
  resp.errno = err_to_errno(err);
  msgsend(sock->pid, &resp, sizeof(Net_listen_resp_t));
}

static void bsock_wakeup_connect(bsock_t *sock, err_t err) {
  Net_connect_resp_t resp;
  resp.type = NET_CONNECT;
  if (err == ERR_OK) {
    resp.ret = 0;
  } else {
    resp.ret = -1;
  }
  resp.errno = err_to_errno(err);
  msgsend(sock->pid, &resp, sizeof(Net_connect_resp_t));
}

static void bsock_wakeup_accept(bsock_t *sock, err_t err) {
  Net_accept_resp_t resp;
  bsock_t *newsock;
  struct sockaddr_in sin;
  ip_addr_t naddr;
  uint16_t port;
  int ret = 0;
  
  newsock = NULL;
  
  /* Catch previous errors */
  if (ERR_IS_FATAL(sock->last_err)) {
    ret = -1;
    err = sock->last_err;
    goto bsock_wakeup_accept_out;
  }
  
  /* Catch this error */
  if (err != ERR_OK) {
    NETCONN_SET_SAFE_ERR(sock, err); /* Don't overwrite fatal error */
    ret = -1;
    goto bsock_wakeup_accept_out;
  }
  
  /* Pull down teh new socket */
  if ((sock->aq_valid != 0) && (sock->aq_len > 0)) {
    newsock = qget(sock->aq);
    sock->aq_len -= 1;
    /* Confirm accept */
    tcp_accepted(tcp_conn(sock));
    /* Register event with callback */
    API_EVENT(sock, NETCONN_EVT_RCVMINUS, 0);
  }
  
  
  /* Check for abort */
  if (newsock == NULL) {
    /* connection has been aborted or something */
    NETCONN_SET_SAFE_ERR(sock, ERR_ABRT);
    ret = -1;
    err = ERR_ABRT;
    goto bsock_wakeup_accept_out;
  }
  
  /* get the IP address and port of the remote host */
  err = bsock_getaddr(newsock, &naddr, &port, BSOCK_GETADDR_REMOTE);
  if (err != ERR_OK) {
    BSOCK_LOG("[BSOCK] bsock_getaddr() failed; err=%d\n", err);
    bsock_do_delconn(newsock);
    bsock_free_sockfd(newsock->sockfd);
    ret = -1;
    err = ERR_ABRT; /* FIXME: SOCKET ERROR */
    goto bsock_wakeup_accept_out;
  }
  
  /* Return value is the sockfd. */
  ret = newsock->sockfd;
  
  /* Note that POSIX only requires us to check addr is non-NULL. addrlen must
   * not be NULL if addr is valid.
   */
  if (sock->copy_addr) {
    memset(&sin, 0, sizeof(sin));
    sin.sin_len = sizeof(sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_addr_from_ipaddr(&sin.sin_addr, &naddr);
    
    MEMCPY(&(resp.cliaddr), &sin, sizeof(sin));
    resp.addrlen = sizeof(sin);
  }
  
 bsock_wakeup_accept_out:
  resp.type = NET_ACCEPT;
  resp.ret = ret;
  resp.port = port;
  resp.ip_addr = ntohl( naddr.addr ); 
  resp.errno = err_to_errno(err);
  msgsend(sock->pid, &resp, sizeof(Net_accept_resp_t)); // TODO
}

static void bsock_wakeup_send(bsock_t *sock, err_t err) {
  Net_send_resp_t resp;
  
  /* We're doing sending.  Free the write buffer */
  if (sock->write_buf) {
    free(sock->write_buf);
    sock->write_buf = NULL;
  }
  
  resp.type = NET_SEND;
  if (err != ERR_OK) {
    BSOCK_LOG("BSOCK: send returning -1\n");
    resp.ret = -1;
  } else {
    resp.ret = sock->write_len;
  }
  
  
  resp.errno = err_to_errno(err);
  BSOCK_LOG("[BSOCK] wakeup_send(): ret=%d errno=%d\n",resp.ret,resp.errno);
  msgsend(sock->pid, &resp, sizeof(Net_send_resp_t));
}

static void bsock_wakeup_recv(bsock_t *sock, err_t err) {
  Net_recv_resp_t resp;
  bsock_copy_info_t copy_info;
  int i;
  int ret;
  
  BSOCK_FUNC_ENTER();
  
  /* Catch previous errors */
  if (ERR_IS_FATAL(sock->last_err)) {
    ret = -1;
    err = sock->last_err;
    goto bsock_wakeup_recv_out;
  }
  
  /* Catch this error */
  if (err != ERR_OK) {
    NETCONN_SET_SAFE_ERR(sock, err); /* Don't overwrite fatal error */
    ret = -1;
    goto bsock_wakeup_recv_out;
  }
  
  
  /* FIXME THIS SUCKS! Did we get here through an error */
  if (sock->rq_len == 0) {
    err = sock->last_err;
    if (err == ERR_CLSD) {
      ret = 0;
    } else {
      ret = -1;
    }
    BSOCK_LOG("[BSOCK] wakeup_recv(): rq_len = 0... error? ");
    goto bsock_wakeup_recv_out;
  }
  
  /* 
   * Copy data into the response message.  This is a bit complicated,
   * because there is one case where the packet needs to stay at the
   * head of hte queue.  There's no enqueue-on-head function for our
   * queues, so we do a qsearch and use the copy_info structure to
   * keep track of how much we've copied and how many pbufs we need to
   * dequeue at the end.
   */
  memset(&resp, 0, sizeof(Net_recv_resp_t));
  memset(&copy_info, 0, sizeof(bsock_copy_info_t));
  copy_info.sock = sock;
  copy_info.resp = &resp;
  copy_info.recvlen = sock->recv_len;
  copy_info.bytes_copied = 0;
  copy_info.pbufs_to_dequeue = 0;
  copy_info.last_off = sock->last_off;
  
  //	BSOCK_LOG("[Bsock] wkp_rcv() precopy: fd=%d recvlen=%d bs_cpd=%d ptd=%d last_off=%d\n", sock->sockfd, copy_info.recvlen, copy_info.bytes_copied, copy_info.pbufs_to_dequeue, copy_info.last_off);

  /* Do the copying.  NOTE: This updates some fields in copy_info */
  qsearch(sock->rq, &bsock_copy_recv, &copy_info);
  
  BSOCK_LOG("[BSOCK] wkp_rcv() postcopy: recvlen=%d bs_cpd=%d ptd=%d last_off=%d\n", copy_info.recvlen, copy_info.bytes_copied, copy_info.pbufs_to_dequeue, copy_info.last_off);

  /* Update the tcp window */
  if (sock->stype == NETCONN_TCP)
    tcp_recved(tcp_conn(sock), copy_info.bytes_copied);
  
  /* Dequeue and free data, unless user is peeking */
  if ((sock->recv_flags & MSG_PEEK) == 0) {
    BSOCK_LOG("[BSOCK] wkp_recv() updating ");
    /* copy_info.pbufs_to_dequeue now holds how many packets we need to dq. */
    for(i = 0; i < (copy_info.pbufs_to_dequeue); i++) {
      void *buf;
      
      /* Dequeue buf */
      buf = qget(sock->rq);
      
      /* Free it */
      if (sock->stype == NETCONN_TCP) {
	pbuf_free((struct pbuf *)buf);
      } else {
	netbuf_delete((struct netbuf *)buf);
      }
    }
    
    /* Update rq_len */
    sock->rq_len -= copy_info.bytes_copied;
    
    /* Update last offset, in case part of a pbuf is leftover */
    sock->last_off = copy_info.last_off;
  }
  
  err = ERR_OK;
  ret = copy_info.bytes_copied;
  
#if 0
  /* Check to see from where the data was.*/
  ip_addr_t fromaddr;
  if (from && fromlen) {
    struct sockaddr_in sin;
    
    if (netconn_type(sock->conn) == NETCONN_TCP) {
      addr = &fromaddr;
      netconn_getaddr(sock->conn, addr, &port, 0);
    } else {
      addr = netbuf_fromaddr((struct netbuf *)buf);
      port = netbuf_fromport((struct netbuf *)buf);
    }
    
    memset(&sin, 0, sizeof(sin));
    sin.sin_len = sizeof(sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_addr_from_ipaddr(&sin.sin_addr, addr);
    
    if (*fromlen > sizeof(sin)) {
      *fromlen = sizeof(sin);
    }
    
    MEMCPY(from, &sin, *fromlen);
    
  }
#endif
  
 bsock_wakeup_recv_out:
  
  /* No more recv request */
  sock->recv_len = 0;
  sock->recv_flags = 0;
  
  /* Send message back to user */
  resp.hdr.type = NET_RECV;
  resp.hdr.sockfd = sock->sockfd;
  resp.hdr.ret = ret;
  resp.hdr.errno = err_to_errno(err); /* FIXME sockets */
  msgsend(sock->pid, &resp, sizeof(Net_recv_hdr_t) + copy_info.bytes_copied);
}

static int bsock_copy_recv(void *elementp, const void *keyp) {
  struct pbuf *p;
  bsock_copy_info_t *copy_info;
  bsock_t *sock;
  int copy_whole_packet;
  int buflen, copylen;
  
  copy_info = (bsock_copy_info_t *)keyp;
  sock = copy_info->sock;
  
  if (sock->stype == NETCONN_TCP) {
    p = (struct pbuf *)elementp;
  } else {
    p = ((struct netbuf *)elementp)->p;
  }
  
  /* If we are closed, we indicate that we no longer wish to use the socket */
  if (p == NULL) {
    BSOCK_LOG("[BSOCK] copy_recv(): Found null packet; should close\n");
    API_EVENT(sock, NETCONN_EVT_RCVMINUS, 0);
    /* Avoid to lose any previous error code */
    NETCONN_SET_SAFE_ERR(sock, ERR_CLSD);
    return 1;
  }
  
  /* Figure out how much to copy */
  buflen = p->tot_len;
  buflen -= copy_info->last_off;
  
  if ((copy_info->recvlen) >= buflen) {
    copy_whole_packet = 1;
    copylen = buflen;
  } else {
    copy_whole_packet = 0;
    copylen = copy_info->recvlen;
  }
  
  /* Copy into buffer */
  pbuf_copy_partial(p, (uint8_t*)(copy_info->resp->buf) + (copy_info->bytes_copied),
		    copylen, copy_info->last_off);
  
  /* If this is the first packet we've copied, also need to copy addr info */
  if (copy_info->pbufs_to_dequeue == 0) {
    ip_addr_t fromaddr;
    uint16_t port;
    struct sockaddr_in *sin;
    
    /* Get remote IP and port */
    if (sock->stype == NETCONN_TCP) {
      fromaddr =  ip_conn(sock)->remote_ip;
      port     = tcp_conn(sock)->remote_port;
    } else {
      fromaddr = ((struct netbuf *)elementp)->addr;
      port     = ((struct netbuf *)elementp)->port;
    }
    
    /* Save it into the response msg */
    sin = (struct sockaddr_in *)(&(copy_info->resp->hdr.src));
    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_len = sizeof(struct sockaddr_in);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    inet_addr_from_ipaddr(&(sin->sin_addr), &fromaddr);
    
    copy_info->resp->hdr.addrlen = sizeof(struct sockaddr_in);
    
    BSOCK_LOG("[BSOCK] Copying addr data into resp\n");
    BSOCK_LOG("[BSOCK] src: %s:%d\n", inet_ntoa(sin->sin_addr),ntohs(sin->sin_port));
    
  }
  
  /* Update copy_info to reflect what we just did */
  copy_info->bytes_copied += copylen;
  if (sock->stype == NETCONN_TCP) {
    /* Update last_off so we know where to pick up on the next recv() */
    if (copy_whole_packet)
      copy_info->last_off = 0;
    else
      copy_info->last_off += copylen;
    
    /* Update how much is left to be copied */
    copy_info->recvlen -= copylen;
    
    /* Did we copy this entire pbuf? */
    copy_info->pbufs_to_dequeue += copy_whole_packet;
  } else {
    /* This is udp or raw, so just throw away what we didn't get. */
    copy_info->last_off = 0;
    copy_info->pbufs_to_dequeue += 1;
    copy_info->recvlen = 0;            /* To indicate that we're done */
  }
  
  /* Are we done? */
  return ((copy_info->recvlen) <= 0);
}

static void bsock_wakeup_ghbn(bsock_t *sock, err_t err) {
  //Net_ghbn_resp_t resp;
#if 0
  resp.type = NET_CONNECT;
  resp.ret = err;
  msgsend(sock->pid, &resp, sizeof(Net_connect_resp_t));
#endif
}

static void bsock_wakeup_close(bsock_t *sock, err_t err) {
  Net_close_resp_t resp;
  int pid;
  
  pid = sock->pid;
  bsock_free_sockfd(sock->sockfd);
  
  resp.type = NET_CONNECT;
  if (err == ERR_OK)
    resp.ret = 0;
  else
    resp.ret = -1;
  resp.errno = err_to_errno(err);
  msgsend(pid, &resp, sizeof(Net_close_resp_t));
}

static err_t bsock_getaddr(bsock_t *sock, ip_addr_t *addr, uint16_t *port, uint8_t local) {
  err_t err = ERR_OK;
  
  /* Has this sock experienced a major catastrophe */
  if ((ip_conn(sock)) == NULL) {
    err = ERR_CONN;
    goto bsock_getaddr_out;
  }
  
  /* ip addr is same no matter conn type */
  *addr = local ? ip_conn(sock)->local_ip : ip_conn(sock)->remote_ip;
  
  /* Get the port (or the protocol, in the case of raw conn) */
  switch(NETCONNTYPE_GROUP(sock->stype)) {
#if LWIP_RAW
  case NETCONN_RAW:
    if (local) {
      *port = raw_conn(sock)->protocol;
    } else {
      err = ERR_CONN;
    }
    break;
#endif /* LWIP_RAW */
#if LWIP_UDP
  case NETCONN_UDP:
    if (local) {
      *port = udp_conn(sock)->local_port;
    } else {
      if (((udp_conn(sock)->flags) & UDP_FLAGS_CONNECTED) == 0) {
	err = ERR_CONN;
      } else {
	*port = udp_conn(sock)->remote_port;
      }
    }
    break;
#endif /* LWIP_UDP */
#if LWIP_TCP
  case NETCONN_TCP:
    *port = local ? tcp_conn(sock)->local_port : tcp_conn(sock)->remote_port;
    break;
  }
#endif /* LWIP_TCP */
  
  
 bsock_getaddr_out:
  return err;
}

/**
 * Callback registered in the netconn layer for each socket-netconn.
 * Processes recvevent (data available) and wakes up tasks waiting for select.
 */
static void bsock_event_callback(bsock_t *sock, enum netconn_evt evt, uint16_t len) {
  
  //	BSOCK_FUNC_ENTER();
#if 0 /* FIXME: select() not implemented */
  int s;
  struct lwip_select_cb *scb;
  int last_select_cb_ctr;
  SYS_ARCH_DECL_PROTECT(lev);
  
  LWIP_UNUSED_ARG(len);
  
  /* Get socket */
  if (conn) {
    s = conn->socket;
    if (s < 0) {
      /* Data comes in right away after an accept, even though
       * the server task might not have created a new socket yet.
       * Just count down (or up) if that's the case and we
       * will use the data later. Note that only receive events
       * can happen before the new socket is set up. */
      SYS_ARCH_PROTECT(lev);
      if (conn->socket < 0) {
	if (evt == NETCONN_EVT_RCVPLUS) {
	  conn->socket--;
	}
	SYS_ARCH_UNPROTECT(lev);
	return;
      }
      s = conn->socket;
      SYS_ARCH_UNPROTECT(lev);
    }
    
    sock = get_socket(s);
    if (!sock) {
      return;
    }
  } else {
    return;
  }
  
  SYS_ARCH_PROTECT(lev);
  /* Set event as required */
  switch (evt) {
  case NETCONN_EVT_RCVPLUS:
    sock->rcvevent++;
    break;
  case NETCONN_EVT_RCVMINUS:
    sock->rcvevent--;
    break;
  case NETCONN_EVT_SENDPLUS:
    sock->sendevent = 1;
    break;
  case NETCONN_EVT_SENDMINUS:
    sock->sendevent = 0;
    break;
  case NETCONN_EVT_ERROR:
    sock->errevent = 1;
    break;
  default:
    LWIP_ASSERT("unknown event", 0);
    break;
  }
  
  if (sock->select_waiting == 0) {
    /* noone is waiting for this socket, no need to check select_cb_list */
    SYS_ARCH_UNPROTECT(lev);
    return;
  }
  
  /* Now decide if anyone is waiting for this socket */
  /* NOTE: This code goes through the select_cb_list list multiple times
     ONLY IF a select was actually waiting. We go through the list the number
     of waiting select calls + 1. This list is expected to be small. */
  
  /* At this point, SYS_ARCH is still protected! */
 again:
  for (scb = select_cb_list; scb != NULL; scb = scb->next) {
    if (scb->sem_signalled == 0) {
      /* semaphore not signalled yet */
      int do_signal = 0;
      /* Test this select call for our socket */
      if (sock->rcvevent > 0) {
	if (scb->readset && FD_ISSET(s, scb->readset)) {
	  do_signal = 1;
	}
      }
      if (sock->sendevent != 0) {
	if (!do_signal && scb->writeset && FD_ISSET(s, scb->writeset)) {
	  do_signal = 1;
	}
      }
      if (sock->errevent != 0) {
	if (!do_signal && scb->exceptset && FD_ISSET(s, scb->exceptset)) {
	  do_signal = 1;
	}
      }
      if (do_signal) {
	scb->sem_signalled = 1;
	/* Don't call SYS_ARCH_UNPROTECT() before signaling the semaphore, as this might
	   lead to the select thread taking itself off the list, invalidagin the semaphore. */
	sys_sem_signal(&scb->sem);
      }
    }
    /* unlock interrupts with each step */
    last_select_cb_ctr = select_cb_ctr;
    SYS_ARCH_UNPROTECT(lev);
    /* this makes sure interrupt protection time is short */
    SYS_ARCH_PROTECT(lev);
    if (last_select_cb_ctr != select_cb_ctr) {
      /* someone has changed select_cb_list, restart at the beginning */
      goto again;
    }
  }
  SYS_ARCH_UNPROTECT(lev);
#endif /* if 0 */
}

static void print_timestamp() {
  struct timespec tm;
  // clock_gettime(CLOCK_MONOTONIC, &tm); TODO
  printf("%ld sec %ld nsec\n",tm.tv_sec,tm.tv_nsec);
}



static int lookup(void* ep,const void *skeyp) {
  if ( (((bsock_t*)ep)->sockfd) == (*((int*)skeyp)))
    return TRUE;
  else
    return FALSE;
}
