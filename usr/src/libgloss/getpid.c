/* 
 * getpid.c -- get the current process id.
 */

#include <_ansi.h>
#include <string.h>    
#include <msg.h> 
#include <time.h>
#include <syscall.h>
#include <stdio.h>

int
_DEFUN (getpid, (),
        )
{
  Getpid_req_t req;
  Getpid_resp_t resp;
  Msg_status_t status;
  memset(&status,-1,sizeof(Msg_status_t));
  memset(&resp,-1,sizeof(Getpid_resp_t));
  req.type = SC_GETPID;
  msgsend(SYS,&req,sizeof(Getpid_req_t));
  msgrecv(SYS,&resp, sizeof(Getpid_resp_t), &status);

  if ((resp.type != SC_GETPID) || 
      (status.src != SYS) || 
      (status.msgsize_orig != sizeof(Getpid_resp_t))) {
    printf("getpid error: resp.type=%d, status.src=%d, msgsize_orig=%d\n",
	   resp.type, status.src, status.msgsize_orig);
  }

  return resp.pid;
}
