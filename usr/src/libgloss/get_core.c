/* get_core.c -- gets running core number.
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

/*
 * get core number
 */
int
_DEFUN (get_core, (),
				)
{
        Get_core_req_t req;
        Get_core_resp_t resp;
        Msg_status_t status;
        req.type = SC_CORE;

	/* Issue the system call */
        msgsend(SYS, &req, sizeof(Get_core_req_t));
        /* msgrecv necessary because msgsend does not block. */
        msgrecv(SYS, &resp, sizeof(Get_core_resp_t), &status);


	/* return the number value of the core the system is running on
	/* value is provided by systask_do_get_core in systask.c				*/
	return resp.ret_val;
}
