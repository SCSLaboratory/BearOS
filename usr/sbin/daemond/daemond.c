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
 * daemond.c -- The daemond implementation
 */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <syscall.h>		/* system calls */
#include <msg.h>		/* msgsend/msgrecv */
#include <unistd.h>		/* sleep */
#include <stdint.h>

#include <utils/bool.h>

#include <sbin/daemond.h>		/* daemon interface */

/* The daemond message buffer */
static daemond_msg_t msgbuff;

/* Daemons may not use printf */
//#define DAEMOND_DEBUG 1     /* show debugging */
//#define DAEMOND_SHOW 1      /* show errors */

/* 
 * main -- The main loop of the daemon
 *
 * Every message recieves a reply - typically DACK or DNAK
 */
int main(void) {
  Msg_status_t status;		/* status of a recv */
  int done,ready,replysize;
  daemond_msg_t *msgp,*replyp;

  msgp = replyp = &msgbuff;	/* buffer reused for reply */
  /* 
   * while not done { receive msg, service it, respond to sender } 
   */
  for(done=FALSE, ready=FALSE; !done ; ) {
    /* default reply type is same as the recieved message */
    /* set the default reply size based on generic response */
    replysize=sizeof(generic_dresp_t); 
    /* block and wait to recieve a message */
    msgrecv(ANY,(void*)msgp, sizeof(msgbuff), &status); /* recieve */
    /* service the message and set up the reply */
#ifdef DAEMOND_DEBUG
    daemond_print_req(stdout,"daemond-recv",msgp);
#endif 
    switch(msgtype(msgp)) {	/* what message type arrived */
    case DINIT:			/* init */
      ready=TRUE;		/* set ready when init complete */
    case DPING:			/* ping */
      respvalue(replyp)=DACK;	/* default reply is ACK */
      if(!ready)		/* send NAK if not ready */
	respvalue(replyp)=DNAK;	
      break;
    case DQUIT:			/* quit for testing only */
      if(ready)	{		/* only quit if ready */
	done=TRUE;		
	respvalue(replyp)=DACK; /* repond with ACK */
      }
      else
	respvalue(replyp)=DNAK; /* respond with NAK */
      break;
    default:			/* error! */
#ifdef DAEMOND_SHOW
      fprintf(stderr,"[error on recv: %d]\n",msgtype(msgp));
#endif
      respvalue(replyp)=DNAK;	/* respond with NAK */
      break;
    }
    /* reply to sender using message specific size */
    msgsend(status.src,(void*)replyp, replysize); 
#ifdef DAEMOND_DEBUG
     daemond_print_resp(stdout,"daemond-send",replyp);
#endif 
  }
  exit(EXIT_SUCCESS);
}
