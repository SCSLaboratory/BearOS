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
 * tnfsd2.c -- The nfsd regression test for directory operations
 *
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>
#include <fcntl.h>

#include <sys/wait.h>		/* waitpid */

#include <sbin/nfsd.h>		/* nfsd interface */
#include <sbin/daemon_msg_types.h>

#define MAXLINE 128
#define MAXARGS 20
#define OVERWRITE 1
#define TRUE 1
#define FALSE 0
#define ROUNDS 10		/* do ROUNDS pings as a test */
#define PINGDELAY 1		/* seconds between pings */

#define TNFSD_SHOW 1          /* set this to show progress and errors */

/* local helper functions */
static int startDaemon(char *cmd);
static int parseArgs(char *line, char *argv[]);
static void errorexit(char *errorstr);
static void check_data(void *buffp,int startpoint,int bytesrequested,int bytesread);
static void clear_data(void *buffp,int sz);
static void set_data(void *buffp,int sz);

/* 
 * tnfsd -- Regression test for a nfsd.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */

int main(int argc, char *argv[]) {
  int dpid,dirh,res;			/* daemon pid, return code */
  int wstatus,i,dfh,filesize,bytesread; 
  char filenm[MAXPATHLEN];
  char dirfilenm[MAXPATHLEN];
  void *buffp;
  uint64_t blocksz,blockpsn;
  struct stat *statp;

  fflush(stdout);
  if(argc==1)
    dpid=NFSD;			/* initialized by shell */
  else if(argc==2) {
    dpid = start_daemon(argv[1]);
    /* initialize the daemon */
    if(nfsd_init(dpid)<0) 
      errorexit("[Unable to start nfsd]\n");
  }
  else
    errorexit("Usage: tnfsd [nfsdif]\n");

  /* ping it */
  if(nfsd_ping(dpid)<0)
    errorexit("[Unable to ping nfsd]\n");

  /*
   * check directory operations on 4 directories
   */
  for(i=0; i<ROUNDS; i++) {
    /* create a directory name: bar0,bar1,... bar3 */
    sprintf(filenm,"%s%d","/bar",i); 

    /* STAT a non-existant directory*/
    if((res=nfsd_stat(dpid,filenm,&statp))>=0)
      errorexit("[Stat on non-existant directory succeeded!]\n");

    /* make the directory */
    if(nfsd_mkdir(dpid,filenm)<0) 
      errorexit("[Unable to make directory]\n");

    /* STAT the new directory*/
    if((res=nfsd_stat(dpid,filenm,&statp))<0)
      errorexit("[Stat on new directory failed!]\n");

    /* read the directory */
    if((dirh=nfsd_opendir(dpid,filenm))<0) 
      errorexit("[Unable to open directory]\n");

    do { 
      if(((res=nfsd_readdir(dpid,dirh,dirfilenm))<0) && res!=DNAK)
	errorexit("[Unable to read directory]\n");
    } while (res!=DNAK);

    if(nfsd_closedir(dpid,dirh)<0) 
      errorexit("[Unable to close directory]\n");

    if(nfsd_rmdir(dpid,filenm)<0) 
      errorexit("[Unable to remove directory]\n");

    /* STAT a non-existant directory*/
    if((res=nfsd_stat(dpid,filenm,&statp))>=0)
      errorexit("[Stat on removed directory succeeded!]\n");
  }

  if(dpid!=NFSD) {
    /* terminate the daemon */
    if(nfsd_quit(dpid)<0) 
      errorexit("[Unable to quit nfsd]\n");
    /* wait for daemon to terminate */
    res=waitpid(dpid,&wstatus,WUNTRACED);
    if(!res)
      errorexit("[tnfsd: error on termination]\n");
  }
  /* responded correctly */
  exit(EXIT_SUCCESS);
}


/*
 * start_daemon -- start a single process All daemons start with no
 * args or environ and recieve an init message
 */
int start_daemon(char *cmdp) {
  int pid,argc;
  char cmd[MAXLINE]; 		/* local copy of cmd */
  char *argv[MAXARGS];		/* argv for cmd */
  char errorstr[MAXLINE];

  strcpy(cmd,cmdp);		     /* make a local copy of cmd */
  if((argc=parseArgs(cmd,argv))==0)  /* parse the copy */
    errorexit("[Parsing Error!]\n"); /* should never happen */
  pid=fork();			/* create a new process */
  switch(pid) {			/* decide what to do with it */
  case -1:			/* error */
    errorexit("[Unable to fork command]\n");
    break;
  case 0:			/* child: execute the command */
    if(execve(cmd,argv,NULL)<0) {
      sprintf(errorstr,"[Unable to execute: %s]\n",cmd);
      errorexit(errorstr);
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

/* helper routines */

static void errorexit(char *errorstr) {
#ifdef TNFSD_SHOW
  printf("%s",errorstr);
#endif
  exit(EXIT_FAILURE);
}

