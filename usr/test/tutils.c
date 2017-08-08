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
 * tutils.c -- utilities used by multiple tests
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/tutils.h>

void* make_person(char* namep, int age,int datasize) {
  person_t* pp;
  int i;

  if(!(pp = (person_t*)malloc(sizeof(person_t)))) {
    printf("[Error: malloc failed allocating person]\n");
    exit(EXIT_FAILURE);
  }
  if(!(pp->bigarray = (double*)malloc(datasize*sizeof(double)))) {
    printf("[Error: malloc failed allocating array]\n");
    exit(EXIT_FAILURE);
  }
  strcpy(pp->person_name,namep);
  pp->person_age=age;
  for(pp->person_checksum=0, i=0; i<datasize; i++) {
    bigarrayval(pp,i)=(double)age; /* put the age in every element */
    pp->person_checksum+=(double)age; /* form a checksum */
  }
  return (void*)pp;
}

void free_person(void *ep) {
  person_t* pp=(person_t*)ep;

  free(pp->bigarray);
  free(pp);
}

int check_person(void *ep,char *namep,int age,int datasize) {
  int ret,i;
  double checksum;
  person_t* pp=(person_t*)ep;

  for(checksum=0, i=0; i<datasize; i++)
    checksum += bigarrayval(pp,i);
  if((strcmp(pp->person_name,namep)!=0) ||   /* name was ok */
     (pp->person_age!=age) ||		     /* age was ok */
     (pp->person_checksum!=checksum) ||	     /* adds up ok */
     (pp->person_checksum!=(age*datasize))) /* same value in every slot */
    return FALSE;
  return TRUE;
}

void print_person(void *ep) {
  person_t* pp=(person_t*)ep;

  if(ep!=NULL)
    printf("[%s,%d,%lf]\n",pp->person_name,pp->person_age,pp->person_checksum);
  else
    printf("[No person]\n");
}

void dotme(int n,int i) {	/* print 5 dots in the course of a test */
  int v;

  v=n/5;
  if(v>0 && (i%v)==0) {		/* print a dot if i is at 1/5 of n */
    printf(".");
    fflush(stdout);
  }
}


/* NOTE: To search a queue you need a boolean function
 * and a key. The boolean function is applied to every element 
 * of a queue until an element that matches the key is found.
 * In order for this to work on arbitrary structures, all arguments
 * are passed as void*.
 */
int is_age(void *ep,const void *keyp) {
  int result;
  
  /* NOTE: First coerce the arguments to be correct types */
  person_t* pp=(person_t*)ep;
  int* ap=(int*)keyp;
  
  /* Then do the actual comparison and return a result */
  if(pp->person_age == *ap) {
    result = TRUE;
  }
  else
    result = FALSE;
  return result;
}

#define MAXLINE 128
#define MAXARGS 20

static int parse_args(char *line, char *argv[]);

/*
 * startDaemon -- start a single process
 * All daemons start with no args or environ and recieve an
 * init message
 */
int start_daemon(char *cmdp) {
  int pid,argc;
  char cmd[MAXLINE]; 		/* local copy of cmd */
  char *argv[MAXARGS];		/* argv for cmd */

  strcpy(cmd,cmdp);		      /* make a local copy of cmd */
  if((argc=parse_args(cmd,argv))==0) { /* parse the copy */
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
