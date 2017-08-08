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

#pragma once
/******************************************************************************
 * 
 * File: network.h
 *
 * Description: Contains API to the network code.
 *
 *****************************************************************************/

#include <sbin/netd.h>	/* For Pkt_msg_t */
/*
 * Some basic Ethernet constants.
 */
#define	ETHER_ADDR_LEN		6	/* length of an Ethernet address */
#define	ETHER_TYPE_LEN		2	/* length of the Ethernet type field */
#define	ETHER_CRC_LEN		4	/* length of the Ethernet CRC */
#define	ETHER_HDR_LEN		(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)
#define	ETHER_MIN_LEN		64	/* minimum frame len, including CRC */
#define	ETHER_MAX_LEN		1518	/* maximum frame len, including CRC */
#define	ETHER_MAX_LEN_JUMBO	9018	/* max jumbo frame len including CRC */

#define ETHER_VLAN_ENCAP_LEN 4  /* len of 802.1Q VLAN encapsulation */

#define	ETHERMTU	(ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMTU_JUMBO	(ETHER_MAX_LEN_JUMBO - ETHER_HDR_LEN - ETH

#ifdef BEAR_USERLAND_NET
typedef struct {
	unsigned int ifnum;
	void *state;
	uint8_t eaddr[6];
	int (*initfn)(void*);
	void (*sendfn)(void*,Pkt_msg_t*);
} knetif_t;
#endif

/* Copied definitions for kernel - userland separation purposes */

#define ARP_TMR_INTERVAL 500

#define DHCP_COARSE_TIMER_SECS 60 

#define DHCP_COARSE_TIMER_MSECS (DHCP_COARSE_TIMER_SECS * 1000UL)

#define DHCP_FINE_TIMER_MSECS 10

#define TCP_TMR_INTERVAL       10

/**************************** FUNCTIONS *********************************/

/* Start network support code. Should be called once during init. */
int network_init();

/* Add a new if. */
void *network_ifadd(void *sc, int (*init_fn)(void*), 
		    void (*send_fn)(void*,Pkt_msg_t*));

/* Called when netd sends msg to init a driver. */
int network_ifinit(unsigned int ifnum, uint8_t *eaddr);

/* Called when netd sends a packet that must go out. */
void network_output_pkt(Pkt_msg_t*);

