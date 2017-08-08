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
#include <unistd.h>		/* sleep */
#include <sys/wait.h>		/* waitpid */
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>

#define MAXLINE 256
#define MAXARGS 100

static int executecmd();
static int parseArgs(char *line, char *argv[]);

extern char **environ;

/* 
 * tfork -- just forks processes and kills them forever.
 */
int main(int argc, char *argv[]) {
  int ret;
  ret=executecmd();
  return ret;
}

static int executecmd() {
  int pid,endpid,status;
  char cmd[MAXLINE]; 		/* local copy of cmd */
  int argc;			/* argc for cmd */
  char *argv[MAXARGS];		/* argv for cmd */
  int res;		

  //  sprintf(cmd,"dot %d",i);	/* run the dot program */
  sprintf(cmd,"dot");	/* run the dot program */
  if((argc=parseArgs(cmd,argv))==0) { /* parse the copy */
    printf("[Parsing Error!]\n");     /* should never happen */
    return(EXIT_FAILURE);
  }
  
  res=EXIT_SUCCESS;		/* assume command succeeds */
  pid=fork();			/* create a new process */
  switch(pid) {			/* decide what to do with it */
  case -1:			/* error */
    printf("[Unable to fork command]\n");
    res=EXIT_FAILURE;
    break;
  case 0:			/* child: execute the command */
    if(execve(cmd,argv,environ)<0) { /* NULL works */
      printf("[Unable to execute: %s]\n",argv[0]);
      exit(EXIT_FAILURE);	/* EXIT THE CHILD -- its done */
    }
    break;				/* child exits */
  default:				/* parent */
    //    endpid=waitpid(pid,&status,WUNTRACED); /* wait for child */
    //    res=status;
    break;			/* parent exits */
  }
  return res;
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
