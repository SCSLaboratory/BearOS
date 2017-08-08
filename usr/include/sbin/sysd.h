#pragma once
/*
 * sysd.h -- The sysd interface 
 */
#include <_ansi.h>
#include <stdio.h>
#include <syscall.h>
#include <limits.h>
#include <msg.h>		/* Msg_status_t */
#include <sbin/daemon_msg_types.h>	/* generic message types */
#include <sbin/syspid.h>

/* 
 * Daemon Message Types: 
 *
 * daemon_msg_types.h provides:
 * DINIT=1
 * DPING=2
 * DQUIT=3 
 */

/*
 * Daemon Specific Message Formats.
 */

/* Daemon Message Buffer -- aligned on a 64 bit boundary */
typedef union { 
  generic_dresp_t dresp;	
  generic_dping_req_t dping;	
  generic_dquit_req_t dquit;	
} sysd_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

/* Deamon Printing Interface -- used to print messages */
void sysd_print_req_msg(FILE *logp,const char *op,const sysd_msg_t *msgp);
void sysd_print_resp_msg(FILE *logp,const char *op,const sysd_msg_t *respp);

/* 
 * Daemon Communications Interface -- used to communcation with sysd 
 *
 * All responses are of the form <type,value>; where value=DNAK on error.
 */
int sysd_ping(int sysd_pid);	/* responds DACK and continues */
int sysd_quit(int sysd_pid);	/* responds DACK and quits */




