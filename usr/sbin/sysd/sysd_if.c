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
 * sysd_if.c -- this file provides a communications interface for the
 * system daemon sysd.  Prototypes are available throught the
 * interface sysd.h.
 */
#include <stdio.h>		   /* fprintf */
#include <stdint.h>		   /* uint8_t */
#include <sbin/daemon_msg_types.h> /* generic message formats */
#include <sbin/sysd.h>		   /* sysd daemon interface */

//#define SYSD_IF_DEBUG 1 	/* set this to see all messages */

static sysd_msg_t msgbuff;
static const sysd_msg_t *msgbuffp = &msgbuff;

/* helper function */
static int send_recv_generic(int sysd_pid,int msgsize);
#ifdef SYSD_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes);
#endif


/*
 * The generic interface provides init/ping/quit messages Only ping &
 * quit are used by the system daemon, generic response signifies
 * success.
 */
int sysd_ping(int sysd_pid) {
  generic_dping_req_t *msgp;

  msgp = (generic_dping_req_t*)msgbuffp;  
  msgtype(msgp) = DPING;
  return send_recv_generic(sysd_pid,sizeof(generic_dping_req_t));
}

int sysd_quit(int sysd_pid) {
  generic_dquit_req_t *msgp;

  msgp = (generic_dquit_req_t*)msgbuffp;  
  msgtype(msgp) = DQUIT;
  return send_recv_generic(sysd_pid,sizeof(generic_dquit_req_t));
}

/*
 * send_recv_generic -- assumes a message of the form <type,...> has
 * been set up the msgbuff, sends it, then waits for a generic
 * response of the form <type,val>, then returns the value.
 */
static int send_recv_generic(int sysd_pid,int msgsize) {
  Msg_status_t status;
  generic_dresp_t *replyp;

  /* send to sysd */
  msgsend(sysd_pid,(void*)msgbuffp,msgsize);
#ifdef SYSD_IF_DEBUG
  sysd_print_req(stdout,"send to sysd:",msgbuffp); /* print msg */
#endif

#ifdef SYSD_IF_DEBUG
  /* clear the buffer if debugging */
  clear_buff((uint8_t*)msgbuffp,sizeof(generic_dresp_t));
#endif

  /* recv from sysd */
  replyp = (generic_dresp_t*)msgbuffp;
  msgrecv(sysd_pid,(void*)replyp,sizeof(generic_dresp_t),&status);
#ifdef SYSD_IF_DEBUG
  sysd_print_resp(stdout,"recv from sysd:",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}


#ifdef SYSD_IF_DEBUG
/* clear the msg buffer between send and recv if debugging */
static clear_buff(uint8_t *p,uint64_t numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *p = 0;
}
#endif 

