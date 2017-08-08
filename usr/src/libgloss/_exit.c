/* Stub version of _exit.  */

#include <_syslist.h>
#include <_ansi.h>
#include <string.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>
#include <stdio.h>
#include <sbin/sysd.h>

_VOID
_DEFUN (_exit, (rc),
	int rc)
{
  Exit_req_t req;
  Exit_resp_t resp;
  Msg_status_t status;
  int pid;

  req.type = SC_EXIT;
  req.exit_sig = rc;
  msgsend(SYS, &req, sizeof(Exit_req_t));
  /* msgrecv necessary because msgsend does not block. */
  //msgrecv(SYS, &resp, sizeof(Exit_resp_t), &status);
}
