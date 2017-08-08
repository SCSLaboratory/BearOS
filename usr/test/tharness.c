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
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <sys/wait.h>		/* waitpid */
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>

#include <daemond.h>		/* generic daemon interface */

#define MAXLINE 128
#define MAXARGS 20
#define OVERWRITE 1
#define TRUE 1
#define FALSE 0

//#define HARNESS_DEBUG 1  /* uncomment to show messages */

/* The Harness Message Buffers */
static uint8_t msg[DAEMOND_MAXMSG];
static uint8_t reply[DAEMOND_MAXMSG]; 

/* local helper functions */
static int startProcess(char *cmd);
static int parseArgs(char *line, char *argv[]);
static void send_recv(int dpid, int msgsize);


/* 
 * tharness -- A test harness to run a daemon and its test
 * concurrently.  The harness sends an initial message to the test to
 * inform it of the process id used by the daemon for testing. 
 *
 * Starts both processes, waits for them to terminate, and returns
 * EXIT_SUCCESS if both succeed, otherwise EXIT_FAILURE
 */
int main(int argc, char *argv[]) {
  int hpid,dpid,tpid;		/* harness, daemon & test pids */
  int drc,trc;			/* daemon & rest return codes */
  int dstatus,tstatus;
  int tn;

  fflush(stdout);
  if(argc!=4 || ((tn=atoi(argv[1]))<=0)) {
    printf("Usage: tharness testnumber <daemond> <tdaemond>\n");
    printf("Daemon Harness: runs a daemon and its test code\n");
    exit(EXIT_FAILURE);
  }
  tpid = startProcess(argv[3]);	
  dpid = startProcess(argv[2]);
  /* send test an INIT with pid of daemon to test process */
  type(msg) = TESTINIT;
  dpid(msg) = dpid;
  testnum(msg) = tn;
  send_recv(tpid,TESTINITSIZE);

  /* wait for test and daemon to terminate */
  if(tpid)
    trc=waitpid(tpid,&tstatus,WUNTRACED);
  if(dpid)
    drc=waitpid(dpid,&tstatus,WUNTRACED);
  if(!drc || !trc) {
    printf("[Harness: error on termination\n");
    exit(EXIT_FAILURE);
  }
    
  return(EXIT_SUCCESS);
}

/*
 * startProcess -- start a single process
 * All daemons start with no args or environ and recieve an
 * init message
 */
int startProcess(char *cmdp) {
  int pid,argc;
  char cmd[MAXLINE]; 		/* local copy of cmd */
  char *argv[MAXARGS];		/* argv for cmd */

  strcpy(cmd,cmdp);		      /* make a local copy of cmd */
  if((argc=parseArgs(cmd,argv))==0) { /* parse the copy */
    printf("[Parsing Error!]\n");     /* should never happen */
    return(EXIT_FAILURE);
  }
  pid=fork();			/* create a new process */
  switch(pid) {			/* decide what to do with it */
  case -1:			/* error */
    printf("[Unable to fork command]\n");
    break;
  case 0:			/* child: execute the command */
    if(execve(cmd,argv,NULL)<0) {
      printf("[Unable to execute: %s]\n",cmd);
      exit(EXIT_FAILURE);	/* exit the child only */
    }
    break;
  default:			/* parent: pid designates child */
    break;
  }
  return pid;
}

/*
 * parseArgs -- parses line into argv
 *    returns: number of args or 0 if more than MAXARGS
 */
static int parseArgs(char *line, char *argv[]) {
  int argc;			/* number of arguments */
  char *p;			/* pointer to a token */
  int lastarg;

  lastarg=MAXARGS-1;
  for(argc=0; argc<lastarg && ((p=strsep(&line," \t"))!=NULL); ) 
    if(*p!='\0' && argc<lastarg) {
      argv[argc]=p;
      argc++;
    }
  argv[argc]=NULL;  		/* null terminate argv[] */
  return argc;
}


/* 
 * send_recv -- sends the outgoing message and blocks to recieve a
 * reply.  Causes test program to exit & fail if an error is detected.
 */
static void send_recv(int dpid, int msgsize) {
  Msg_status_t status;

#ifdef HARNESS_DEBUG 
  print_daemond_msg(stdout,"harness",SENDDIR,dpid,msg); /* print msg */
#endif 

  msgsend(dpid,(void*)msg,msgsize);
  msgrecv(ANY,(void*)reply,DAEMOND_MAXMSG,&status); /* block */

#ifdef HARNESS_DEBUG 
  print_daemond_msg(stdout,"harness",RECVDIR,status.src,reply); /* print msg */
#endif 

  if(status.src != dpid || type(reply)!=ACK) { /* error? */
    printf("[harness: recv errror (%d,%d,%d)]\n",
	   status.src,dpid,type(reply));
    exit(EXIT_FAILURE);
  }
}

