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
 * statd_if.c -- this file contains the implementation for 
 * functions that communicate with statd. The prototypes are
 * available throught the interface statd.h.
 */
#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* uint8_t */
#include <msg.h>		/* Msg_status_t */

#include <sbin/daemon_msg_types.h>	/* generic message types */
#include <sbin/statd.h>		/* daemon interface */

//define STATD_IF_DEBUG 1 	/* set this to see messages in test*/

#ifdef STATD_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes);
#endif

/* helper function */
static int send_recv_generic(int dpid,statd_msg_t *msgbuffp,int msgsize);

/*
 * The generic interface provides init/ping/quit messages
 */
int statd_init(int dpid,statd_msg_t *msgbuffp) {
  generic_dinit_req_t *msgp;

  msgp = (generic_dinit_req_t*)msgbuffp;  
  msgtype(msgp) = DINIT;
  return send_recv_generic(dpid,msgbuffp,sizeof(generic_dinit_req_t));
}

int statd_ping(int dpid,statd_msg_t *msgbuffp) {
  generic_dping_req_t *msgp;

  msgp = (generic_dping_req_t*)msgbuffp;  
  msgtype(msgp) = DPING;
  return send_recv_generic(dpid,msgbuffp,sizeof(generic_dping_req_t));
}

int statd_quit(int dpid,statd_msg_t *msgbuffp) {
  generic_dquit_req_t *msgp;

  msgp = (generic_dquit_req_t*)msgbuffp;  
  msgtype(msgp) = DQUIT;
  return send_recv_generic(dpid,msgbuffp,sizeof(generic_dquit_req_t));
}

/*
 * send_recv_generic -- assumes a message has been set up to send in
 * the msgbuff, sends it, then waits for a generic response and
 * returns the value associated with it.
 */
static int send_recv_generic(int dpid,statd_msg_t *msgbuffp,int msgsize) {
  Msg_status_t status;
  generic_dresp_t *replyp;

  /* send to statd */
  msgsend(dpid,(void*)msgbuffp,msgsize);
#ifdef STATD_IF_DEBUG
  statd_print_req(stdout,"tstatd-send",msgbuffp); /* print msg */
#endif

  /* recv from statd */
  replyp = (generic_dresp_t*)msgbuffp;
#ifdef STATD_IF_DEBUG
  clear_buff((uint8_t*)replyp,sizeof(generic_dresp_t));
#endif
  msgrecv(dpid,(void*)replyp,sizeof(generic_dresp_t),&status);
#ifdef STATD_IF_DEBUG
  statd_print_resp(stdout,"tstatd-recv",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}

#ifdef STATD_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *p = 0;
}
#endif 

