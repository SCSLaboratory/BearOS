#pragma once

/*
 * driver_api.h -- header file for the driver wrapper in the net task.
 */

#include <stdint.h>
#include <sbin/lwip/netif.h>
#include <sbin/lwip/err.h>

/*
 * the minimal driver info kept around in the net task.  Basically
 *  allows the net task to send pkt messages to the right place.
 */
typedef struct {
	struct netif *ifp;
	int pid;
} Driver_api_softc_t;

err_t driver_linkoutput(struct netif *ifp, struct pbuf *p);
err_t driver_init(struct netif *ifp);

