
#pragma once
/*
 * daemon_msg_types.h -- generic msg formats available to ALL daemons
 *
 * These types are typically used to begin implementation using the 
 * create_daemon script.
 */

/*
 * Common Daemon Message types: To ensure efficient switching on message types, they
 * are always defined in ASCENDING order beginning with 1.  These 3
 * represent the minimum set of messages.
 */
#define DINIT 1			/* initalize deamon */
#define DPING 2			/* Deamon ping */
#define DQUIT 3			/* quit deamon */

/* Generic Daemon Response values */
#define DACK 0			/* Daemon ACK */
#define DNAK -1			/* Daemon NAK */

/* 
 * Generic Response type 
 * NOTE: A message specific response is defined
 * ONLY if the generic response is inadequate.
 */
typedef struct {
  int type;			/* type of message being responded to */
  unsigned int tag;
  int respvalue;	       	/* value of response - typically ACK/NAK */
} generic_dresp_t;

/*
 * Message/Response Pairs
 */

/* INIT message/resonse */
typedef struct {
  int type;
  unsigned int tag;
} generic_dinit_req_t;

/* generic_init_resp_t -- not required, generic_resp_t */


/* PING message/response */
typedef struct {
  int type;
  unsigned int tag;
} generic_dping_req_t;

/* generic_ping_resp_t -- not required, generic_resp_t */


/* QUIT message/response */
typedef struct {
  int type;
  unsigned int tag;
} generic_dquit_req_t;

/* generic_ping_resp_t -- not required, generic_resp_t */

typedef union { 
  generic_dresp_t dresp;	/* generic response */
  generic_dinit_req_t dinit;	/* init request */
  generic_dping_req_t dping;	/* ping request */
  generic_dquit_req_t dquit;	/* quit request */
} generic_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

/*
 * Access Macros: message "type" is always an int at the beginning of
 * every message
 */
#define msgtype(msgp) (*((int*)msgp)) /* first field of EVERY message */
#define respvalue(msgp) (((generic_dresp_t*)msgp)->respvalue) /* value in the daemon response */
