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
 * tsysd.c -- The sysd regression test
 *
 * Each Daemon has an associated test.  Eventually, every daemon will
 * have its own log; for the time being, only enable printing from
 * either the test or the daemon, but not both. Each has its own DEBUG
 * flag to allow you to choose which ONE will do printing. They both
 * use the same printing routine for messages in sysd_utils.c.
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <stdint.h>
#include <unistd.h>
#include <string.h>		/* strsep */
#include <newlib.h>
#include <msg.h>		/* msgsend/msgrecv */
#include <syscall.h>

#include <sys/wait.h>		/* waitpid */
#include <sbin/syspid.h>	/* daemon pids */
#include <sbin/sysd.h>		/* sysd interface */

#include <utils/tutils.h>	/* start_daemon */
#include <utils/bool.h>		/* TRUE/FALSE */

#define ROUNDS 3		/* number of rounds (mulitple of 5) */

#define SYSD_SHOW_ERRORS 1
#define SYSD_SHOW_PROGRESS 1

void error_exit(char *msg,int expect,int val) { 
#ifdef SYSD_SHOW_ERRORS
  printf("FAIL: [%s,expect %d, recv %d]\n",msg,expect,val); 
#endif
  exit(EXIT_FAILURE); 
}

void success(char *msg, int val) {
#ifdef SYSD_SHOW_PROGRESS
  printf("OK:   [%s,%d]\n",msg,val); 
#endif
}

/* 
 * tsysd -- Regression test for a sysd.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */
int main(int argc, char *argv[]) {
  int i,j,sysd_pid,pid,id,res;

  sysd_pid=SYSD;

  /* PING the daemon ROUNDS times */
  for(i=0; i<ROUNDS; i++) {
    if((res=sysd_ping(sysd_pid))!=DACK) 
      error_exit("ping",DACK,res);
    success("ping",i);
  }

  /* responded correctly */
  exit(EXIT_SUCCESS);
}

