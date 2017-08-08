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
 * tstatd.c -- The statd regression test
 *
 * Each Daemon has an associated test.  Eventually, every daemon will
 * have its own log; for the time being, only enable printing from
 * either the test or the daemon, but not both. Each has its own DEBUG
 * flag to allow you to choose which ONE will do printing. They both
 * use the same printing routine for messages in statd_utils.c.
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>

#include <sys/wait.h>		/* waitpid */

#include <sbin/statd.h>		/* statd interface */
#include <utils/tutils.h>	/* start_daemon */
#include <utils/bool.h>

#define ROUNDS 5		/* do 5 pings as a test */

//#define TSTATD_DEBUG 1 	/* set this to see messages in test*/
#define NO_DEBUG 1 	        /* set only if no debugging in test or daemon */

/* The Daemon Message Buffer -- always aligned on a 64 bit boundary */
static statd_msg_t msgbuff __attribute__ ((aligned(sizeof(uint64_t))));
static void send_recv(int dpid, int msgsize, statd_msg_t *msgp);

/* 
 * tstatd -- Regression test for a statd.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */
int main(int argc, char *argv[]) {
  int dpid,drc;			/* daemon pid and return code */
  int tn;			/* the testnumber */
  statd_msg_t *msgp, *replyp;	/* pointers to a msg and response */
  int status;
  int i;

  fflush(stdout);
  if(argc!=3 || ((tn=atoi(argv[1]))<=0)) {
    printf("Usage: tstatd testnumber <statd>\n");
    exit(EXIT_FAILURE);
  }

  msgp = replyp = &msgbuff;	/* msg and response use same buffer */

  /* grab test number */
  fprintf(stdout,"[%d: statd test",tn);
#ifndef NO_DEBUG
  printf("]\n");		/* if debugging test output msgs on newline */
#endif 
  fflush(stdout);
  dpid = start_daemon(argv[2]);

  /* just ping the daemon ROUNDS times */
  for(i=0; i<ROUNDS; i++) {
    msgtype(msgp) = DPING;
    send_recv(dpid,sizeof(generic_dping_req_t),&msgbuff);
    /* delay between pings */
#ifdef NO_DEBUG
    printf("."); 		/* no debugging, let us know its working */
#endif
    fflush(stdout);
  }

  /* then tell it to quit */
  msgtype(msgp) = DQUIT;
  send_recv(dpid,sizeof(generic_dquit_req_t),&msgbuff);
#ifdef NO_DEBUG
  printf("]");			/* no debugging, let us know when its done */
#endif
  fflush(stdout);

  /* wait for daemon to terminate */
  if(dpid)
    drc=waitpid(dpid,&status,WUNTRACED);
  if(!drc) {
    printf("[tstatd: error on termination\n");
    exit(EXIT_FAILURE);
  }

  /* responded correctly */
  exit(EXIT_SUCCESS);
}

static void send_recv(int dpid, int msgsize, statd_msg_t *msgp) {
  Msg_status_t status;
  statd_msg_t *replyp=msgp;

  /* send to daemon */
  msgsend(dpid,(void*)msgp,msgsize);
#ifdef TSTATD_DEBUG
  print_statd_msg(stdout,"send",DREQUEST,msgp); /* print msg */
#endif

  /* recv from daemon */
  msgrecv(ANY,(void*)replyp,sizeof(generic_dresp_t),&status);
#ifdef TSTATD_DEBUG
  print_statd_msg(stdout,"recv",DRESPONSE,replyp); /* print reply */
#endif

  if(status.src != dpid || respvalue(replyp)!=DACK) { /* error? */
    printf("[tstatd: recv errror (%d,%d,%d)]\n",
	   status.src,dpid,respvalue(replyp));
    exit(EXIT_FAILURE);
  }
}

