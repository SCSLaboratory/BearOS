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
/*
 * daemond_if.c -- this file contains the implementation for 
 * functions that communicate with daemond. The prototypes are
 * available throught the interface daemond.h.
 */
#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* uint8_t */
#include <sbin/daemond.h>		/* daemon interface */

//define DAEMOND_IF_DEBUG 1 	/* set this to see messages in test*/

static daemond_msg_t msgbuff;
static const daemond_msg_t *msgbuffp = &msgbuff;

#ifdef DAEMOND_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes);
#endif

/* helper function */
static int send_recv_generic(int dpid,int msgsize);

/*
 * The generic interface provides init/ping/quit messages
 */
int daemond_init(int dpid) {
  generic_dinit_req_t *msgp;

  msgp = (generic_dinit_req_t*)msgbuffp;  
  msgtype(msgp) = DINIT;
  return send_recv_generic(dpid,sizeof(generic_dinit_req_t));
}

int daemond_ping(int dpid) {
  generic_dping_req_t *msgp;

  msgp = (generic_dping_req_t*)msgbuffp;  
  msgtype(msgp) = DPING;
  return send_recv_generic(dpid,sizeof(generic_dping_req_t));
}

int daemond_quit(int dpid) {
  generic_dquit_req_t *msgp;

  msgp = (generic_dquit_req_t*)msgbuffp;  
  msgtype(msgp) = DQUIT;
  return send_recv_generic(dpid,sizeof(generic_dquit_req_t));
}

/*
 * send_recv_generic -- assumes a message has been set up to send in
 * the msgbuff, sends it, then waits for a generic response and
 * returns the value associated with it.
 */
static int send_recv_generic(int dpid,int msgsize) {
  Msg_status_t status;
  generic_dresp_t *replyp;

  /* send to daemond */
  msgsend(dpid,(void*)msgbuffp,msgsize);
#ifdef DAEMOND_IF_DEBUG
  daemond_print_req(stdout,"generic-send",msgbuffp); /* print msg */
#endif

#ifdef DAEMOND_IF_DEBUG
  clear_buff((uint8_t*)msgbuffp,sizeof(generic_dresp_t));
#endif

  /* recv from daemond */
  replyp = (generic_dresp_t*)msgbuffp;
  msgrecv(dpid,(void*)replyp,sizeof(generic_dresp_t),&status);
#ifdef DAEMOND_IF_DEBUG
  daemond_print_resp(stdout,"generic-recv",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}

#ifdef DAEMOND_IF_DEBUG
/* A message buffer is used for both sending and recieving. As a
 * result, when debugging, its good practice to clear the buffer to
 * between these operations to ensure that no spurious data is used in
 * communication.
 */
static clear_buff(uint8_t *p,uint64_t numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *p = 0;
}
#endif 

