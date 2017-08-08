#include <_ansi.h>
#include <_syslist.h>
#include <string.h>	/* userland */
#include <msg.h>	/* userland */
#include <time.h>
#include <syscall.h>

int
_DEFUN (fork, (),
        _NOARGS)
{
	Fork_req_t req;
	Fork_resp_t resp;

	memset( &req, 0, sizeof(Fork_req_t) );
	memset( &resp, 0, sizeof(Fork_resp_t) );

	Msg_status_t status;
	req.type = SC_FORK;
	msgsend(SYS,&req,sizeof(Fork_req_t));
	msgrecv(SYS,&resp,sizeof(Fork_resp_t),&status);
	return resp.ret;
}
