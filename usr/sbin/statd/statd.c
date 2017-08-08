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
 * statd.c -- The statd implementation
 */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <syscall.h>		/* system calls */
#include <msg.h>		/* msgsend/msgrecv */
#include <unistd.h>		/* sleep */
#include <stdint.h>

#include <utils/bool.h>

#include <sbin/statd.h>		/* daemon interface */
#include <sbin/nfsd.h>		/* NFSD */

//#define STATD_DEBUG 1
//#define STATD_SHOW 1

#define DELAY 120

static generic_msg_t msgbuff;	/* The statd message buffer */
static const generic_msg_t *msgp = &msgbuff;

/* 
 * main -- statd waits for an init from the shell, responds with an
 * ack, then loops pinging nfsd every 2 minutes.
 */
int main(int argc,char *argv[]) {
  Msg_status_t status;		/* status of a recv */
  int done,msgsize;
  int nfsd_pid,mypid;

  mypid=getpid();
  if(mypid!=STATD) {
#ifdef STATD_SHOW
    printf("[statd: initialized with pid %d, expected %d\n",mypid,STATD);
#endif
    exit(EXIT_FAILURE);
  }
  if(argc==2) 			/* invoked from command line */
    nfsd_pid = atoi(argv[1]);	/* get the pid of the nfs daemon */
  else				/* invoked from system */
    nfsd_pid = NFSD;		/* use the nfs daemon id */

  /* block to recieve a message from the shell */
  msgrecv(ANY,(void*)msgp, sizeof(msgbuff), &status);
  switch(msgtype(msgp)) {	/* what message arrived */
  case DINIT:			/* good to go */
      respvalue(msgp)=DACK;	/* ACK the shell */      
      msgsize=sizeof(generic_dresp_t); 
      msgsend(status.src,(void*)msgp,msgsize); 
      while(1) {		/* start pinging the nfs server */
	if(nfsd_ping(nfsd_pid)!=DACK) {
#ifdef STATD_SHOW
	  fprintf(stderr,"[NFSD Session Closed!]\n");
#endif
	  exit(EXIT_FAILURE);
	}
	else {
	  // printf("#");  	/* let me know you are there! */
	  // fflush(stdout);
	  sleep(DELAY);		/* wait for 120 secs */
	}
      }
      break;
  default:			/* error! */
#ifdef STATD_SHOW
    fprintf(stderr,"[statd: error on recv: %d]\n",msgtype(msgp));
#endif
    exit(EXIT_FAILURE);
    break;
  }
  exit(EXIT_SUCCESS);
}

