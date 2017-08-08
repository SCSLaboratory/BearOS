/* kill.c -- remove a process.
 *
 * Copyright (c) 1995 Cygnus Support
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#include <_ansi.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>
#include <sbin/sysd.h>

/*
 * kill -- go out via exit...
 */
int
_DEFUN (kill, (pid, signo),
        int pid _AND 
        int signo)
{

  Kill_req_t req;
  Kill_resp_t resp;
  Msg_status_t status;

  /* Only kill PID > 0 */
  if(pid<=0)
    return -1; /* return failure */

  req.type     = resp.type = SC_KILL;
  req.kill_sig = signo;
  req.kill_pid = pid;

  req.tag = resp.tag = get_msg_tag();

  msgsend(SYS, &req, sizeof(Kill_req_t));

  /* msgrecv necessary because msgsend does not block. */
  msgrecv(SYS, &resp, sizeof(Kill_resp_t), &status);
  /* return 0 for process killed return 1 for process trying to be
   * killed not existing. Return value is provided by systask_do_kill
   * in systask.c
   */

  return resp.ret_val;
}
