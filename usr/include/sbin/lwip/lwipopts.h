#ifndef __LWIP_OPTS_H__
#define __LWIP_OPTS_H__	

/* (ST) TURN ON LOOPBACK */
#define LWIP_HAVE_LOOPIF 1
#define LWIP_NETIF_LOOPBACK 1

/* Debug options */
#define LWIP_DBG_ON 0x80U
#define LWIP_DBG_TYPES_ON (LWIP_DBG_LEVEL_SERIOUS | LWIP_DBG_LEVEL_SEVERE)

#define LWIP_DEBUG					1
#define ETHARP_DEBUG                LWIP_DBG_ON
//#define NETIF_DEBUG                 LWIP_DBG_ON
//#define PBUF_DEBUG                  LWIP_DBG_ON
//#define ICMP_DEBUG                  LWIP_DBG_ON
//#define IP_DEBUG                    LWIP_DBG_ON
//#define DHCP_DEBUG                  LWIP_DBG_ON
//#define ARP_DEBUG                   LWIP_DBG_ON
//#define TCP_DEBUG                     LWIP_DBG_ON
#define UDP_DEBUG                     LWIP_DBG_ON
/*#define MEMP_OVERFLOW_CHECK         2 */


/*#define SYS_LIGHTWEIGHT_PROT		1 *//* Enable sys.h's macros for prot/unprot */

/* define the size of the static pbuf allocation */
#define PBUF_POOL_SIZE              4096 /*changed by steve k was 512 */
#define PBUF_POOL_BUFSIZE			1550 /*adjust this to |dbuf| */

/* This makes lwIP use OUR malloc (aka kmalloc) for everything except mempools. */
#define MEM_LIBC_MALLOC             1
#define MEM_SIZE					(1024 * 1024 * 100) /*changed by steve k  was time 10 */

#define MEMP_NUM_PBUF                   64
#define MEMP_NUM_RAW_PCB                32
#define MEMP_NUM_UDP_PCB                32
#define MEMP_NUM_TCP_PCB                32
#define MEMP_NUM_TCP_PCB_LISTEN         32
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_REASSDATA              16
#define MEMP_NUM_FRAG_PBUF              15
#define MEMP_NUM_ARP_QUEUE              30
#define MEMP_NUM_SYS_TIMEOUT            8
#define MEMP_NUM_NETBUF                 32
#define MEMP_NUM_TCPIP_MSG_INPKT        32

#define LWIP_DHCP					1
#define LWIP_ARP                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_RAW                    1

#define ARP_QUEUEING                1

#define IP_REASS_MAX_PBUFS          64
#define IP_FRAG_USES_STATIC_BUF     0
#define IP_DEFAULT_TTL              255
#define IP_SOF_BROADCAST            1
#define IP_SOF_BROADCAST_RECV       1

#define LWIP_ICMP                   1
#define LWIP_BROADCAST_PING         1
#define LWIP_MULTICAST_PING         1
#define LWIP_RAW                    1

#define TCP_WND                     (4 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            (4 * TCP_SND_BUF/TCP_MSS)
#define TCP_LISTEN_BACKLOG          1

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HWADDRHINT       1
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 0

#endif /* __LWIP_OPTS_H__ */
