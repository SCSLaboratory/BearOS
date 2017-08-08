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
 * File: driver_api.c
 *
 * Description: Converts lwIP's driver calls into messages destined for the
 *              driver process, and vice-versa.
 *
 ******************************************************************************/

#include <msg.h>	/* For msgsend and msgrecv */
#include <msg_types.h>
#include <string.h>

#include <sbin/lwip/err.h>
#include <sbin/lwip/etharp.h>
#include <sbin/lwip/sockets.h>
#include <sbin/lwip/netdb.h>

#include <sbin/driver_api.h>
#include <sbin/network.h>
#include <sbin/netd.h>	/* For netd's packet definitions */

#ifndef USER
#include <kstdio.h>
#endif /* USER */
#ifdef USER
#include <stdio.h>
#endif /*USER */

err_t driver_linkoutput(struct netif *ifp, struct pbuf *p) {
	Driver_api_softc_t *sc;
	Pkt_msg_t pkt;

	sc = (Driver_api_softc_t*)ifp->state;

	pkt.type = NDRV_OUTPKT;
	pkt.ifnum = ifp->num;
	pkt.len = p->tot_len;

	/* Copies whole packet into flat buffer. */
	if (pbuf_copy_partial(p,pkt.pkt, p->tot_len, 0) != p->tot_len) {
		printf("[Net] Error: Could not copy packet into pkt\n");
		return ERR_BUF;
	}

	msgsend(sc->pid, &pkt, sizeof(Pkt_hdr_t) + pkt.len);

	return ERR_OK;
}

err_t driver_init(struct netif *ifp) {
	Ndrv_init_req_t req;
	Ndrv_init_resp_t resp;
	Msg_status_t status;
	Driver_api_softc_t *sc;

	sc = (Driver_api_softc_t*)ifp->state;

	/* Send message and do driver init. */
	req.type = NDRV_INIT;
	req.ifnum = ifp->num;

	msgsend(sc->pid, &req, sizeof(Ndrv_init_req_t));
	msgrecv(sc->pid, &resp, sizeof(Ndrv_init_resp_t), &status);

	if (resp.ret != 0) {
		printf("[NET] Error: driver_init failed\n");
		return (err_t)resp.ret;
	}

	/* name */
	ifp->name[0] = 'e';
	ifp->name[1] = 't';
	ifp->name[2] = '\0';
	ifp->num     = ifp->num;

	/* Driver functions */
	ifp->linkoutput = &driver_linkoutput;
	ifp->output     = &etharp_output; 

	/* Flags */
	ifp->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
	ifp->mtu = ETHERMTU;

	/* Give MAC address to netif. */
	ifp->hwaddr_len = ETHER_ADDR_LEN;
	memcpy(ifp->hwaddr,resp.eaddr, ETHER_ADDR_LEN);

	return (err_t)resp.ret;
}

