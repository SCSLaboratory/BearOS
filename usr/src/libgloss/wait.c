/*
 * Stub version of wait.
 */

#include <_ansi.h>
#include <_syslist.h>
#include <string.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>

int
_DEFUN (wait, (stat),
        int  *stat)
{
        Waitpid_req_t req;
        Waitpid_resp_t resp;
        Msg_status_t status;

        req.type = SC_WAITPID;
        req.wpid = ANY;

	/* Issue the system call */
        msgsend(SYS, &req, sizeof(Waitpid_req_t));
        msgrecv(SYS, &resp, sizeof(Waitpid_resp_t), &status);

        if (stat != NULL)
                *stat = resp.exit_val;

        return resp.wpid;	
}
