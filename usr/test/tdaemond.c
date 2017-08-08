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
 * tdaemond.c -- The daemond regression test
 *
 * Each Daemon has an associated test.  Eventually, every daemon will
 * have its own log; for the time being, only enable printing from
 * either the test or the daemon, but not both. Each has its own DEBUG
 * flag to allow you to choose which ONE will do printing. They both
 * use the same printing routine for messages in daemond_utils.c.
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

#include <sbin/syspid.h>	/* defines the daemons id e.g. DAEMOND */
#include <sbin/daemond.h>	/* interface: cf. daemond_if.c & daemond_utils.c */

#include <utils/tutils.h>	/* start_daemon */
#include <utils/bool.h>		/* TRUE/FALSE */

//#define DAEMOND_SHOW_PROGRESS 1

#define ROUNDS 5		/* do 5 pings as a test */

/* 
 * tdaemond -- Regression test for a daemond.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */
int main(int argc, char *argv[]) {
  int i,status,dpid,drc;	/* daemon pid and return code */

  fflush(stdout);
  if(argc!=2) {
    printf("Usage: tdaemond daemond\n");
    exit(EXIT_FAILURE);
  }
  dpid = start_daemon(argv[1]);

  /* initialize the daemon */
  if(daemond_init(dpid)<0) {
    printf("[tdaemond: Unable to init daemond]\n");
    exit(EXIT_FAILURE);
  }
#ifdef DAEMOND_SHOW_PROGRESS
    printf(".");
    fflush(stdout);
#endif
  /* ping the daemon ROUNDS times */
  for(i=0; i<ROUNDS; i++) {
    if(daemond_ping(dpid)<0) {
      printf("[tdaemond: Unable to ping daemond]\n");
      exit(EXIT_FAILURE);
    }
    /* delay between pings */
#ifdef DAEMOND_SHOW_PROGRESS
    printf(".");
    fflush(stdout);
#endif
  }
  /* terminate the daemon */
  if(daemond_quit(dpid)<0) {
    printf("[tdaemond: Unable to quit daemond]\n");
    exit(EXIT_FAILURE);
  }
#ifdef DAEMOND_SHOW_PROGRESS
    printf(".");
    fflush(stdout);
#endif
  /* wait for daemon to terminate */
  if(dpid)
    drc=waitpid(dpid,&status,WUNTRACED);
  if(!drc) {
    printf("[tdaemond: error on termination\n");
    exit(EXIT_FAILURE);
  }

  /* responded correctly */
  exit(EXIT_SUCCESS);
}



