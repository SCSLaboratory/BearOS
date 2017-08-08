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
#define	ETHER_MAX_LEN_JUMBO	9018	/* max jumbo frame len, including CRC */

#define ETHER_VLAN_ENCAP_LEN 4  /* len of 802.1Q VLAN encapsulation */

#define	ETHERMTU	(ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMTU_JUMBO	(ETHER_MAX_LEN_JUMBO - ETHER_HDR_LEN - ETH
