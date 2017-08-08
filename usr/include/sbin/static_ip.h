#pragma once
/*
 * static_ip.h
 *
 * Description: A listing of MAC addresses and static IPs associated with them
 *
 */

#include <stdint.h>
#include <sbin/lwip/sockets.h>

struct node_desc {
	uint8_t hwaddr[6];
	ip_addr_t ip;
	ip_addr_t nm;
	ip_addr_t gw;
};

#define STATIC_IP_TABLE_SIZE 4

extern struct node_desc static_ip_table[STATIC_IP_TABLE_SIZE];
