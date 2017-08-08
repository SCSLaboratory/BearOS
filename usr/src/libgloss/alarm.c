/** Alarm.c
	* Implements the alarm syscall used to schedule a signal being sent after 
	* the specified amount of seconds 
	*/
#include <unistd.h>
#include <syscall.h>
#include <msg.h>

unsigned int alarm(unsigned int seconds) {
	Alarm_req_t req;
	Alarm_resp_t resp;
	Msg_status_t status;

	/** Generate the message sent to the kernel */
	req.type = SC_ALARM;
	req.seconds = seconds;
	msgsend(SYS, &req, sizeof(Alarm_req_t));

	return 0;
}
