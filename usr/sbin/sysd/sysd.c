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
 * Sys.c -- The sysd implementation
 * 
 * See NOTES file.
 */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <syscall.h>		/* system calls */
#include <msg.h>		/* msgsend/msgrecv */
#include <unistd.h>		/* sleep */
#include <stdint.h>
#include <limits.h>		/* INT_MAX */
#include <string.h>

#include <sys/socket.h>
#include <netdb.h>	

#include <utils/bool.h>
#include <utils/shash.h>

#include <sbin/sysd.h>		   /* daemon interface */
#include <sbin/e1000.h>		   /* defines E1000D */
#include <sbin/netd.h>		   /* defines NETD */
#include <sbin/statd.h>		   /* defines STATD */
#include <sbin/nfsd.h>		   /* defines NFSD */
#include <sbin/daemon_msg_types.h> /* shell sends only generic messages */
#include <sbin/daemond.h>     /* functions for sending generic msgs */

#define SYSD_DEBUG 1		/* print all messages recvd and sent */
#define SYSD_SHOW 1           /* show errors */

#define MAXLINE 128
#define MAXARGS 20

/* The sysd message buffer */
static sysd_msg_t msgbuff;

/* helper functions */
static int start_shell(char *cmdp);
static int parse_args(char *line, char *argv[]);

static void daemon_init(int dpid);

/* 
 * main -- The main loop of the daemon
 *
 * Every message recieves a reply - typically DACK or DNAK
 */
int main(void) {
  Msg_status_t status;		/* status of a recv */
  int done,replysize,nextfileid,nextsockid,pid,id,replyval;
  sysd_msg_t *msgp,*replyp;
  E1000_ping_req_t e1000_req;
  E1000_ping_resp_t e1000_resp;
  Netd_ping_req_t netd_req;
  Netd_ping_resp_t netd_resp;

  /* messages to check network is initialized */
  msgp = replyp = &msgbuff;	/* buffer reused for reply */	

#ifndef STANDALONE	/* NOT STANDALONE */
  /* Query E1000D to check its ready */
  e1000_req.type = E1000_PING;
  msgsend(E1000D, &e1000_req, sizeof(E1000_ping_req_t));
  msgrecv(E1000D, &e1000_resp, sizeof(E1000_ping_resp_t), &status); 	

  /* Repeatedly query NETD until its available */
  netd_req.type = NET_PING;
  do {
    msgsend(NETD, &netd_req, sizeof(Netd_ping_req_t));	
    msgrecv(NETD, &netd_resp, sizeof(Netd_ping_resp_t), &status);
  } while(netd_resp.value == 0);

  
#if ( defined(BEARNFS) )
  //  daemon_init(NFSD);	/* wait for nfs to be ready */
  //  daemon_init(STATD);	/* wait for statd to be ready */
#endif /* BEARNFS */
#endif /* not STANDALONE */

  daemon_init(PIPED);   /* wait for piped to be ready */

#ifdef STANDALONE
  if((pid=start_shell("slash"))<0) /* launch some shell */
      fprintf(stderr,"[sysd: unable to start slash]\n");
#else
if((pid=start_shell("shell"))<0) /* launch some shell */
    fprintf(stderr,"[sysd: unable to start shell]\n");
#endif /* STANDALONE */

  replysize=sizeof(generic_dresp_t); /* all generic <type,val> replys */
  /* 
   * while not done { receive msg, service it, respond to sender } 
   */
  for(done=FALSE; !done ; ) {
    /* block and wait to recieve a message */
    msgrecv(ANY,(void*)msgp, sizeof(msgbuff), &status); /* receive */
#ifdef SYSD_DEBUG
    sysd_print_req(stdout,"sysd-recv",msgp);
#endif 
    switch(msgtype(msgp)) {	          /* msg type? */
    case DPING:				  /* ping */
      respvalue(replyp)=DACK;		  /* reply is ACK */
      break;
    case DQUIT:			          /* quit */
      respvalue(replyp)=DACK;		  /* reply is ACK */
      done=TRUE;
      break;
    default:
#ifdef SYSD_SHOW			          /* error! */
      fprintf(stderr,"[sysd:error on recv: %d]\n",msgtype(msgp));
#endif 
      respvalue(replyp)=DNAK;	          /* respond with NAK */
      break;
    }
    /* reply to sender using message specific size */
    msgsend(status.src,(void*)replyp, replysize); 
#ifdef SYSD_DEBUG
     sysd_print_resp(stdout,"sysd-send",replyp);
#endif 
  }
  exit(EXIT_SUCCESS);
}


static int start_shell(char *cmdp) {
  int pid,res;
  int argc;			/* argc for cmd */
  char *argv[MAXARGS];		/* argv for cmd */
  char cmd[MAXLINE]; 		/* local copy of cmd */

  strcpy(cmd,cmdp);		      /* make a local copy of cmd */
  if((argc=parse_args(cmd,argv))==0) { /* parse the copy */
#ifdef SYSD_SHOW
    fprintf(stderr,"[sysd: parsing error!]\n");     /* should never happen */
#endif 
    return(EXIT_FAILURE);
  }
  argv[argc]=NULL;			  /* argv null terminated */
  pid=fork();			/* create a new process */
  switch(pid) {		/* decide what to do with it */
  case -1:	
#ifdef SYSD_SHOW		/* error */
    fprintf(stderr,"[sysd: unable to fork shell]\n");
#endif 
    break;
  case 0:			/* child: execute the command */
    if(execve(cmd,argv,environ)<0) {
#ifdef SYSD_SHOW
      fprintf(stderr,"[sysd: %s - unable to exec]\n",argv[0]);
#endif
      res=FALSE;		/* EXIT THE CHILD -- its done */
    }
    break;
  default:			/* parent: pid==childs pid */
    break;
  }
  return pid;
}

/*
 * parse_args -- parses line into argv
 *    returns: number of args or 0 if more than MAXARGS
 */
static int parse_args(char *line, char *argv[]) {
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
 * Send an init message to a daemon 
 */
static void daemon_init(int dpid) {
  int response;

  /* use the generic interface to send an INIT msg to daemon dpid */
  response=daemond_init(dpid);
  if(response!=DACK) {
#ifdef SYSD_SHOW
    fprintf(stderr,"[sysd: unable to initialize daemon, dpid=%d]\n",dpid);
#endif
    exit(EXIT_FAILURE);
  }
}

