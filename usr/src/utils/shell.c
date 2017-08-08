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
 * shell.c -- a simple shell that operates with NFS
 *
 * Runs commands in foreground or background (using &) 
 * Background processes can be kill'ed.
 *
 * BUILTIN COMMANDS: 
 *     cd <dir>                   -- may use ~ 
 *     pwd, printenv, kill, exit  -- basic commands
 *     : <script> [-l]            -- execute /bin/script, -l = indefinate looping 
 *                                -- "tests" - a script that runs regression tests
 *     <cmd> [&]                  -- execute cmd, &=in background
 *
 * AVAILABLE COMMANDS (in /bin):
 *     touch, ls, cp, mv, rm, mkdir, rmdir,
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
#include <sys/stat.h>
#include <msg.h>
#include <sbin/nfsd.h>

//#define QUIETPROMPT 1		/* set to minimize prompt printing */

#define MAXLINE 128
#define MAXARGS 20
#define OVERWRITE 1
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
static void prompt(void);
static void clearline(char line[]);
static int parse_args(char *line, char *argv[]);
static int execute_cmd(char *cmdp);
static int locate_cmd(char *filenm);
static int buildin_cmd(int argc, char *argv[]);
static void kill_process(int pid);
static inline uint64_t readtsc();
static void printenv();
static void change_directory(int argc,char *argv[]);
static int statdir(char *path);
static void execute_script(int argc, char *argv[]);
static char *fgetline(FILE *fp,char *cmdp);

/* 
 * shell main
 */
int main(int argc,char *argv[]) {
  char prev[MAXLINE];
  char cmd[MAXLINE];
  int res;

  prev[0]='\0';			/* no previous command */
  cmd[0]='\0';			/* no current command */

  /* start in the users home directory */
  setenv("PATH",PATHSTR,OVERWRITE);
  setenv("HOME",HOMESTR,OVERWRITE);
  setenv("LOGNAME",LOGSTR,OVERWRITE);
  setenv("PWD",getenv("HOME"),OVERWRITE);
  statdir(getenv("PWD"));

  while(1) {			/* forever */
    prompt();
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
  fflush(stdout);
  }
  return EXIT_SUCCESS;
}


static void prompt() {
#ifdef QUIETPROMPT
  printf(">");			/* useful for debugging */
#else
  printf("%s> ",getenv("PWD"));
#endif	/* QUIETPROMPT */
  fflush(stdout);
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
 * Execute a single command
 */
static int execute_cmd(char *cmdp) {
  int pid,endpid,status;
  char cmd[MAXLINE]; 		/* local copy of cmd */
  int argc;			/* argc for cmd */
  char *argv[MAXARGS];		/* argv for cmd */
  int foreground,res;		

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
    /* no -- try to locate the binary */
    if(!locate_cmd(argv[0])) {
      printf("%s: not found\n",argv[0]);
      return(EXIT_FAILURE);	/* cant continue */
    }
    pid=fork();			/* create a new process */
    switch(pid) {		/* decide what to do with it */
    case -1:			/* error */
      printf("Unable to fork command\n");
      res=EXIT_FAILURE;
      break;
    case 0:			/* child: execute the command */
      if(execve(cmd,argv,environ)<0){
	printf("%s: command not found\n",argv[0]);
	exit(EXIT_FAILURE);	/* EXIT THE CHILD -- its done */
      }
      break;
    default:			/* parent: pid==child */
      if(foreground) {		/* wait for it */
	endpid=waitpid(pid,&status,0);
	res=status;		/* return its exit status */
      }
      else 			/* just remember it */
	printf("[%d]\n",pid);	/* dont catch return value */
      break;
    }
  }
  return res;
}

static int buildin_cmd(int argc,char *argv[]) {
  int res;		/* true if cmd built in, else false */

  res=TRUE;		/* assume builtin */
  if(streq(argv[0],"kill")) /* kill process */
    kill_process(atoi(argv[1]));
  else if(streq(argv[0],"exit")) { /* exit!! */
    printf("[exit]\n");
    exit(EXIT_SUCCESS);
  }
#ifndef STANDALONE
  else if(streq(argv[0],"cd"))	/* cd */
    change_directory(argc,argv);
  else if(streq(argv[0],"pwd"))	/* pwd */
    printf("%s\n",getenv("PWD"));
  else if(streq(argv[0],"printenv")) /* printenv */
    printenv();
  else if(streq(argv[0],":"))	/* script? */
    execute_script(argc,argv);
#endif
  else				/* not built in */
    res=FALSE;
  return res;
}

static void kill_process(int pid) {
  if(kill((pid_t)pid,SIGKILL)==0)
    printf("%d killed\n",pid);
  else
    printf("No such job: %d\n",pid);
}

static inline uint64_t readtsc() {
  uint32_t lo, hi;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx" );
  return (uint64_t)(lo) | ((uint64_t)(hi) << 32); 
}

static int locate_cmd(char *filenm) {
  int found;			
  char fullfilenm[MAXPATHLEN];
  struct stat *statp;

  found = FALSE;		/* look in bin directory */
  sprintf(fullfilenm,"/bin/%s",filenm);
  if(nfsd_stat(NFSD,fullfilenm,&statp)==0)
    found=TRUE;
  return found;
}

static void execute_script(int argc, char *argv[]) {
  FILE *fp;
  char cmd[MAXLINE];
  char outputline[MAXLINE];
  char scriptnm[MAXLINE];
  int rn;
  uint64_t time1, time2; 
  int loop = FALSE;

  if(argc==3 && strcmp(argv[2],"-l")==0) 
    loop=TRUE;
  if(argc>=2) {
    rn=1;
    do {
      if(locate_cmd(argv[1])) {
	sprintf(scriptnm,"/bin/%s",argv[1]);
	if((fp=fopen(scriptnm,"r"))!=NULL) {					
	  time2=0;
	  time1= readtsc();
	  if(loop)
	    printf("[RUN %d, Executing: %s]\n",rn,argv[1]);
	  else
	    printf("[Executing: %s]\n",argv[1]);
	  while(fgetline(fp,cmd)!=NULL) {
	    if(*cmd!='#') {
	      sprintf(outputline,"%s",cmd);
	      printf("%-40s",outputline);
	      fflush(stdout);
	      if(WEXITSTATUS(execute_cmd(cmd))==EXIT_SUCCESS) 
		printf("[OK]\n");
	      else
		printf("[Fail]\n");
	    }
	  }
	  fclose(fp);
	  time2 = readtsc() - time1;
	  printf("[Script took %lu cycles]\n", time2);
	}
	else {
	  printf("%s: not found\n",scriptnm);
	  loop=FALSE;
	}
      }
      else {
	printf("%s: not a script\n",argv[1]);
	loop=FALSE;
      }
      rn++;
    } while(loop);
  }
}

static void printenv() {
  printf("PWD=%s\n",getenv("PWD"));
  printf("PATH=%s\n",getenv("PATH"));
  printf("LOGNAME=%s\n",getenv("LOGNAME"));
  printf("HOME=%s\n",getenv("HOME"));
}

static void change_directory(int argc,char *argv[]) { 
  char filename[MAXPATHLEN];
  char directory[MAXPATHLEN];

  if(argc > 1) {
    strcpy(filename,argv[1]);
    if(statdir(abspath(filename,directory))<0)
      printf("%s: no such directory\n",directory);
    else
      setenv("PWD",directory,OVERWRITE);
  }
  else
    setenv("PWD",getenv("HOME"),OVERWRITE);
}

int statdir(char *path) {	/* check that the destination exists */
  struct stat *statp;
  return nfsd_stat(NFSD,path,&statp);
}

static char *fgetline(FILE *fp,char *cmdp) {
  char *p;

  clearline(cmdp);
  p=fgets(cmdp,MAXLINE,fp);
  if(p!=NULL)
    *(p+strlen(cmdp)-1)='\0';
  return p;
}


