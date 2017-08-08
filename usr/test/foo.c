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

#include <utils/bool.h>

#define MAXLINE 128

static char cmd[MAXLINE];
static void clearline(char line[]);


int main(void) {
  char cmd[MAXLINE];
  int done;

  cmd[0]='\0';			/* no current command */

  for(done=FALSE; !done; ) {
    printf(">>>");
    fflush(stdout);
    getstr(cmd,MAXLINE);
    clearline(cmd);
    if(*cmd!='\0') {
      if(strcmp(cmd,"exit")==0)
	done=1;
      else {
	printf("[%s]\n",cmd);
	fflush(stdout);
      }
    }
  }
  exit(EXIT_SUCCESS);
}

static void clearline(char line[]) {
  int i;
  for(i=0; line[i]!='\n' && i<MAXLINE; i++)
    ;
  if(line[i]=='\n')
    line[i]='\0';
}
