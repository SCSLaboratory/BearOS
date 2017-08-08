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
 * File: e1000d.c
 *
 * Description: Userland network driver. Sends and receives packets over the
 *              network and manages the network card.  There may be multiple
 *              instances of this driver running, if there are multiple network
 *              cards.
 *
 ******************************************************************************/

#include <stdio.h>	/* printf */
#include <stdlib.h>	/* exit */
#include <malloc.h>
#include <stdint.h>
#include <msg.h>
#include <syscall.h>	/* SYS */
#include <msg_types.h>

#include <sys/socket.h>
#include <netdb.h>

#include <sbin/e1000d.h>
#include <sbin/e1000.h>

int e1000_init();
void e1000_main_loop();

static int e1000_is_up = 0;

/* 
 * Types of messages that may be received. This union provides the minimum
 * buffer size necessary to receive any valid message.
 */
typedef union {
	Hwint_msg_t hwint_msg;
	Ndrv_init_req_t init_req;
	Pkt_msg_t outpkt_msg;
	E1000_ping_req_t pq;
} E1000_msg_t;

/* Message-handling functions */
void e1000_do_hwint   (E1000_msg_t*, Msg_status_t*);
void e1000_do_sendmac    (E1000_msg_t*, Msg_status_t*);
void e1000_do_outpkt  (E1000_msg_t*, Msg_status_t*);

/* To catch anomalous behavior */
void e1000_do_invalid (E1000_msg_t *, Msg_status_t*);

void e1000_do_ping (E1000_msg_t *, Msg_status_t*);

#define E1000_FUNC_ARRAY_SZ 4
/* CAUTION: Order of these functions must match numbers defined in kmsg.h */
void (*func_array[E1000_FUNC_ARRAY_SZ])(E1000_msg_t*, Msg_status_t*) = { e1000_do_hwint, e1000_do_sendmac, e1000_do_outpkt, e1000_do_ping };


/* Start function. */
void main(void) {

	/*printf("in e1000 main before init \n");*/

  if(e1000_init()) {
    printf("an e1000 failed to init\n");
    exit(1);
  }

  e1000_main_loop();
}

int e1000_init() {
	
	Ndrv_config_msg_t msg;
	Net_ifadd_req_t req;
	Ndrv_init_req_t  ireq;
	Msg_status_t status;

	/* 
	 * Pull in configuration message that the kernel's network module sends
	 * upon spawning this process.
	 */
	/* THIS MUST BE THE FIRST THING THAT HAPPENS AFTER KHEAPINIT */
	msgrecv(SYS, &msg, sizeof(Ndrv_config_msg_t), &status);

	/* Tell netd that we're alive. */
	req.type = NET_IFADD;
	msgsend(NETD, &req, sizeof(Net_ifadd_req_t));

	/* Wait for init message. */
	msgrecv(NETD, &ireq, sizeof(Ndrv_init_req_t), &status);
	
	/*enable the card itself */
	if(e1000_hw_init(&msg, (Ndrv_init_req_t *)&ireq ))
		return 1;
	/*
	 * This function just responds to the netd process with an "init 		   
	 * successful" message.  
	 */
	e1000_do_sendmac((E1000_msg_t *)&ireq, &status);

	e1000_is_up = 1;
	return 0;
}


void e1000_main_loop() {
	E1000_msg_t msg;
	Msg_status_t status;
	int *ptype;
	int ret;

	ptype = (int*)(&msg);

	while(1) {
		
		ret = msgrecv(ANY, &msg, sizeof(E1000_msg_t), &status);
				
		if (ret >= 0) {
			if ((*ptype) >= 0 && (*ptype) < E1000_FUNC_ARRAY_SZ) {
				(*func_array[*ptype])(&msg,&status);
			} else {
				e1000_do_invalid(&msg,&status);
			}
		}
	}
}

/*===============================================================*
 *				e1000_do_hwint		 						     *
 * Calls the hardware interrupt service routine 				 *	
 *===============================================================*/
void e1000_do_hwint(E1000_msg_t *msg, Msg_status_t *status) {
	e1000_interrupt();
}

/*===============================================================*
 *				e1000_do_outpkt		 						     *
 * Sends a packet over the wire					 				 *	
 *===============================================================*/
void e1000_do_outpkt(E1000_msg_t *msg, Msg_status_t *status) {
	Pkt_msg_t *pkt;
	pkt = (Pkt_msg_t *)msg;
	e1000_output_pkt(pkt);
}

/*===============================================================*
 *				e1000_do_sendmac		 						     *
 * This responds to the network driver process with our mac addr *	
 * and ifnumber to let it know we're alive because it is waiting *
 * for a response.												 *
 * However,  this is in the message loop because if you refresh  *
 * the driver you'll have to call this again. 
 *===============================================================*/
void e1000_do_sendmac(E1000_msg_t *msg, Msg_status_t *status) {
	Ndrv_init_resp_t resp;
	Ndrv_init_req_t *req;

	req = (Ndrv_init_req_t *)msg;

	e1000_getmac_addr(&resp);
		
	resp.ret = 0;
	
	msgsend(status->src, &resp, sizeof(Ndrv_init_resp_t));
}

/* Called if a message arrives with an invalid type. */
void e1000_do_invalid (E1000_msg_t *msg, Msg_status_t *status) {
	printf("[e1000] ERROR: Invalid msg type = %d src = %d len = %d\n",*((int*)msg),status->src, status->msgsize_orig);
}

void e1000_do_ping (E1000_msg_t *msg, Msg_status_t *status) {
	E1000_ping_resp_t resp;

	resp.value = e1000_is_up;
	msgsend(status->src, &resp, sizeof(E1000_ping_resp_t));
}
