#include <unistd.h>	/* For function prototype */
#include <msg.h>	/* For msgsend */
#include <syscall.h>	/* For SYS */
#include <stdlib.h>	/* For EXIT_SUCCESS and EXIT_FAILURE */

/* Sleeps for the given number of milliseconds. */
int usleep(useconds_t ms) {
        Usleep_req_t req;
        Usleep_resp_t resp;
        Msg_status_t status;

        if (ms <= 0) return EXIT_FAILURE;

        req.type   = SC_USLEEP;
        req.slp_ms = ms;
        msgsend(SYS, &req, sizeof(Usleep_req_t));
        msgrecv(SYS, &resp, sizeof(Usleep_resp_t), &status);

	return EXIT_SUCCESS;
}

