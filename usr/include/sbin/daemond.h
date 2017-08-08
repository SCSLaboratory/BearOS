#pragma once
/*
 * daemond.h -- The generic daemond interface 
 */
#include <_ansi.h>
#include <stdio.h>
#include <syscall.h>
#include <msg.h>		/* Msg_status_t */
#include <sbin/daemon_msg_types.h>	/* generic message types DINIT,DPING,DQUIT */
#include <sbin/syspid.h>		/* must define the daemon id e.g DAEMOND */

/*
 * NOTE: Place daemon specific request & response message formats
 * here. Note that the type field is required.
 *
 * A TYPICAL MESSAGE TYPE:
 *
 * #define MSG1 4
 * #define MSG2 5
 * etc
 *
 * A TYPICAL REQUEST:
 *
 * typedef struct {
 *   int type; 
 *   int foo;
 *   double bar;
 * } daemond_foobar_req_t;
 *
 * A TYPICAL RESPONSE:
 *
 * typedef struct {
 *   int type; 
 *   char x;
 *   int y
 * } daemond_foobar_resp_t;
 *
 */

/* The Daemon Message Buffer -- always aligned on a 64 bit boundary */
typedef union { 
  generic_dresp_t dresp;	/* generic response */
  generic_dinit_req_t dinit;	/* init request */
  generic_dping_req_t dping;	/* ping request */
  generic_dquit_req_t dquit;	/* quit request */
} daemond_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

/* Deamon Printing Interface -- used to print daemond messages */
void daemond_print_req_msg(FILE *logp,char *op,daemond_msg_t *msgp); /* print req msgs */
void daemond_print_resp_msg(FILE *logp,char *op,daemond_msg_t *respp); /* print resp msgs */

/* Daemon Communication Interface -- used to communcation with daemond */
int daemond_init(int dpid); /* one for each message type */
int daemond_ping(int dpid);
int daemond_quit(int dpid);



