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
 * slash.c -- a simple shell for use in STANDALONE mode using
 * only programs loaded into RAMDISK. 
 *
 * Runs commands in foreground or background (using &) 

 *
 * BUILTIN COMMANDS: 
 *     pwd, printenv, kill, exit   -- basic commands
 *     : tests [-l] -- run regression tests, -l = run indefinately
 *     <cmd> [&]    -- execute cmd, & = in background (NOTE: space between cmd and &)
 *
 * AVAILABLE COMMANDS (in /randisk):
 *     ps [-as]  -- a=all, s=silent (ie only to serial out)
 *  
 * Requires the following functions are available:
 *     getenv(), setenv(), fork(), execve(), kill()
 *
 * Does not use malloc.
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <newlib.h>
#include <syscall.h>
#include <utils/bool.h>
#include <msg.h>
#include <syscall.h>


#define MAXLINE 128
#define MAXARGS 20
#define OVERWRITE 1
#define FOREVER 1000000
#define PATHSTR "/bin:.:./bin"
#define HOMESTR "/home/bear"
#define LOGSTR "bear"
#define bgetstr(str) getstr(str,MAXLINE)
#define streq(s1,s2) (strcmp(s1,s2)==0)

/*
 * built in environment -- assumes NFS mount point is /bear, 
 * is mount point should contain /bin and /root
 */
extern char **environ;

/* private functions */
static int execute_cmd(char *cmdp);
static int buildin_cmd(int argc, char *argv[]);
static void clearline(char line[]);
static int parse_args(char *line, char *argv[]);
static void printenv(void);
static void tests(int times);
static void test(char *cmd);

/* 
 * slash main program
 */
int main(int argc,char *argv[]) {
  char prev[MAXLINE];
  char cmd[MAXLINE];
  int res;

  prev[0]='\0';			/* no previous command */
  cmd[0]='\0';			/* no current command */

  /* make believe there is an environment */
  setenv("PATH",PATHSTR,OVERWRITE);
  setenv("HOME",HOMESTR,OVERWRITE);
  setenv("LOGNAME",LOGSTR,OVERWRITE);
  setenv("PWD",getenv("HOME"),OVERWRITE);

  while(1) {			/* forever */
    printf("$ ");		/* prompt */
    fflush(stdout);
    bgetstr(cmd);
    clearline(cmd);
    switch(*cmd) {		/* first character? */
    case '\0': 			/* nothing */
      break;
    case '!':			/* !! -- use previous command */
      if(*(cmd+1)=='!') {
	strncpy(cmd,prev,MAXLINE);
	printf("%s\n",cmd);
      }				/* fall through to execute prev */
    default:			/* save it and execute it */
      strncpy(prev,cmd,MAXLINE);
      res=execute_cmd(cmd);
      break;
    }
  }
  return EXIT_SUCCESS;
}

/* 
 * Execute a single command
 */
static int execute_cmd(char *cmdp) {
  char cmd[MAXLINE]; 		/* local copy of cmd */
  char *argv[MAXARGS];		/* argv for cmd */
  int pid,endpid,status,argc,foreground,res;		
  
  strcpy(cmd,cmdp);		      /* make a local copy of cmd */
  if((argc=parse_args(cmd,argv))==0) { /* parse the copy */
    printf("[Parsing Error!]\n");     /* should never happen */
    return(EXIT_FAILURE);
  }
  foreground = TRUE;			  /* foreground by default */
  if(argc>1 && streq(argv[argc-1],"&")) { /* background? */
    foreground = FALSE;			  /* designate background */
    argc--;				  /* remove & */
  }
  argv[argc]=NULL;			  /* argv null terminated */
  res=EXIT_SUCCESS;		/* assume command succeeds */
  if(!buildin_cmd(argc,argv)) {	/* built into shell? */
    pid=fork();			/* create a new process */
    switch(pid) {		/* decide what to do with it */
    case -1:			/* error */
      printf("Unable to fork command\n");
      res=EXIT_FAILURE;
      break;
    case 0:			/* child: execute the command */
      if(execve(cmd,argv,environ)<0){
	       printf(" command not found\n");//,argv[0]);
	       exit(EXIT_FAILURE);	/* EXIT THE CHILD -- its done */
      }
      break;
    default:			/* parent: pid==child */
      if(foreground) {		/* wait for it */
	endpid=waitpid(pid,&status,WUNTRACED);
	res=WEXITSTATUS(status); /* return its exit status */
      }
      else 			/* just remember it */
	printf("[%d]\n",pid);	/* dont catch return value */
      break;
    }
  }
  return res;			/* return last known result */
}

static int buildin_cmd(int argc,char *argv[]) {
  int res,pid;		/* true if cmd built in, else false */

  res=TRUE;		/* assume builtin */
  /* kill process */
  if(streq(argv[0],"kill") && argc==2 && ((pid=atoi(argv[1]))>0)) { 
    if(kill((pid_t)pid,SIGKILL)==0) /* kill system call */
      printf("%d killed\n",pid);
    else
      printf("%d No such job\n",pid);
  }
  else if(streq(argv[0],"pwd"))	/* pwd */
    printf("%s\n",getenv("PWD"));
  else if(streq(argv[0],"printenv")) /* printenv */
    printenv();
  else if(streq(argv[0],":")) {		    /* do the only script -- tests */
    if((argc==2) && streq(argv[1],"tests"))
      tests(1);
    else if((argc==3) && streq(argv[1],"tests") && streq(argv[2],"-l"))	
      tests(FOREVER);
  }
  else if(streq(argv[0],"exit")) { /* exit -- and into oblivion */
    exit(EXIT_SUCCESS);
  }
  else /* not built in */
    res=FALSE;
  return res;
}

static void clearline(char line[]) {
  int i;
  for(i=0; i<MAXLINE; i++) {
    if(line[i]=='\n') {
      line[i]='\0';
      break;
    }
  }
  
  return;
}

/*
 * parse_args -- parses line into argv
 * returns: number of args or 0 if more than MAXARGS
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

static inline uint64_t readtsc() {
  uint32_t lo, hi;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx" );
  return (uint64_t)(lo) | ((uint64_t)(hi) << 32); 
}

/* surrogate for tests script -- consistent with real shell */ 
static void tests(int times) {
  int runid;
  uint64_t time1, time2; 

  runid=1;
  do {
    time2=0;
    time1= readtsc();
    if(times==FOREVER)
      printf("RUN: %d\n",runid++);
    test("ps -s");    /* output processes and statistics on serial line */
    test("tcmdln");
    test("tcmdln 123");
    test("tcmdln 123 foobar");
    test("tcmdln 123 foobar 3.142");
    test("tcmdln 123 foobar 3.142 bill");
    test("texit 1");
    test("texit 2");
    test("texit 3");
    test("texit 4");
    test("tenv");
    //test("tmalloc 1000 1000000");
    test("tq1");
    test("tq2 800 10000");
    test("tq3");
    test("tq4");
    //test("thash 10000");
    //    test("tshash 10000");
    test("tpiped");
    time2 = readtsc() - time1;
    printf("[Script took %lu cycles]\n", time2);
  } while(times==FOREVER);
}

static void test(char *cmd) {
  printf("%-40s",cmd);
  fflush(stdout);
  if(execute_cmd(cmd)==0)
    printf("[OK]\n");
  else
    printf("[Fail]\n");
}

static void printenv(void) {
  printf("PWD=%s\n",getenv("PWD"));
  printf("PATH=%s\n",getenv("PATH"));
  printf("LOGNAME=%s\n",getenv("LOGNAME"));
  printf("HOME=%s\n",getenv("HOME"));
}
