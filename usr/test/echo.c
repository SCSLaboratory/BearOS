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
#include <stdio.h>
#include <stdlib.h>
#include <msg.h>
#include <syscall.h>

#define MAXMSG 80
/* 
 * echo -- a simple system call test for msgsend() and msgrecv()
 */
int main(int argc, char *argv[]) {
  char buff[MAXMSG];
  Msg_status_t status;
  int destpid;

  if(argc!=2 && argc!=3) {
    printf("Echo server must be running first: echo -s &\n");
    printf("Usage: echo <echoserver-pid> <string>\n");
    printf("<echoserver-pid> obtained from the shell\n");
    return(EXIT_FAILURE);
  }
  if(argc==2 && strcmp(argv[1],"-s")==0) { /* run the server */
    printf("[echo server Started]\n");
    do {
      buff[MAXMSG-1] = '\0';	/* terminator always exists */
      msgrecv(ANY, buff, MAXMSG, &status); /* recieve from anyone */
      printf("[echo server: %s, %d bytes, src=%d]\n",
	     buff,status.bytes_rcvd,status.src);
      msgsend(status.src, buff, status.bytes_rcvd); /* echo it */
    } while(strcmp(buff,"exit")!=0);
    printf("[echo server terminated]\n");
  }
  else {
    destpid=atoi(argv[1]);
    strcpy(buff,argv[2]);
    msgsend(destpid, buff, strlen(buff)+1); /* send string + null */
    buff[0]='\0';			    /* clear the buffer */
    msgrecv(destpid,buff,MAXMSG, &status);	    /* recv */
    printf("[echo response: %s, %d, %d]\n", 
	   buff,status.bytes_rcvd,status.src);
  }
  return(EXIT_SUCCESS);
}

