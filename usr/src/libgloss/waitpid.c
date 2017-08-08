#include <_ansi.h>
#include <_syslist.h>
#include <string.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>

/* Block until the specified process has completed */
int waitpid(int wpid, int *pexit_val, int opts) {
	Waitpid_req_t req;
	Waitpid_resp_t resp;
	Msg_status_t status;

	req.type = resp.type = SC_WAITPID;
	req.wpid = wpid;
	req.opts = opts;
	
	req.tag = resp.tag = get_msg_tag();
	
	/* issue the system call */
	msgsend(SYS, &req, sizeof(Waitpid_req_t));
	msgrecv(SYS, &resp, sizeof(Waitpid_resp_t), &status);

	if (pexit_val != NULL)
		*pexit_val = resp.exit_val;

	return resp.wpid;
}
