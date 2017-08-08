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
 * ps.c -- print the process queue
 *
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <syscall.h>
#include <utils/bool.h>
#include <string.h>

int main(int argc, char *argv[]) {
  Ps_req_t req;
  Ps_resp_t resp;
  Msg_status_t status;
  int i,printall,silent;

  if(argc!=1 && argc!=2) {
    printf("Usage: ps [-as]\n"); /* all or silent */
    exit(EXIT_FAILURE);
  }
  printall=FALSE;
  silent=FALSE;
  if(argc==2 && strcmp(argv[1],"-a")==0)
    printall=TRUE;
  else if(argc==2 && strcmp(argv[1],"-s")==0)
    silent=TRUE;

  req.type = SC_PS;
  msgsend(SYS,&req,sizeof(Ps_req_t)); /* do ps system call */
  msgrecv(SYS,&resp,sizeof(Ps_resp_t),&status); /* get response */
  if(!silent) {
    printf("S  PID    CMD\n");
    for(i=0; i<resp.entries; i++)
      if(printall || (!printall && resp.pid[i]>0)) /* ps or ps -a */
	printf("%c  %-6d %s\n",resp.status[i],resp.pid[i],&resp.procnm[i][0]);
    if(resp.entries==MAX_PS_SZ)
      printf("Number of processes exceeds table\n");
  }
  return(EXIT_SUCCESS);
}
