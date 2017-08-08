#pragma once
/*
 * statd.h -- The statd interface 
 */
#include <sbin/daemon_msg_types.h>	/*  generic message formats */
#include <sbin/syspid.h>

/*
 * NOTE: Place daemon specific request & response message formats here e.g.
 *
 * typedef struct {
 *   int type; 
 *   
 * } statd_init_req_t;
 *
 * typedef struct {
 *   int type; 
 *   
 * } statd_init_resp_t;
 *
 */

/* The Daemon Message Buffer -- always aligned on a 64 bit boundary */
typedef union { 
  generic_dresp_t dresp;	/* generic response */
  generic_dinit_req_t dinit;	/* init request */
  generic_dping_req_t dping;	/* ping request */
  generic_dquit_req_t dquit;	/* quit request */
} statd_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

/* Deamon Printing Interface -- used to print statd messages */
void statd_print_req_msg(FILE *logp,char *op,statd_msg_t *msgp);
void statd_print_resp_msg(FILE *logp,char *op,statd_msg_t *respp);

/* Daemon Communications Interface -- used to communcation with statd */
int statd_init(int dpid,statd_msg_t *msgbuffp);
int statd_ping(int dpid,statd_msg_t *msgbuffp);
int statd_quit(int dpid,statd_msg_t *msgbuffp);


