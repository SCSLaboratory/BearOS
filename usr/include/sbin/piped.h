#pragma once
/*
 * piped.h -- The generic piped interface 
 */
#include <_ansi.h>
#include <stdio.h>
#include <lock.h>
#include <syscall.h>
#include <limits.h>
#include <utils/queue.h>
#include <msg.h>		/* Msg_status_t */
#include <sbin/daemon_msg_types.h>/* generic message types DINIT,DPING,DQUIT */
#include <sbin/syspid.h>	  /* must define the daemon id e.g PIPED */

/* first third for files, second third for sockets, last third for pipes */
#define PIPEIDMIN ((2*(INT_MAX/3))+1)
#define PIPEIDMAX (INT_MAX)
#define is_pipeid(x) (x>=PIPEIDMIN && x<=PIPEIDMAX)

#define MAXOPEN      50
#define PIPE_MSG_LEN 512
#define PIPE_LENGTH  51200

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
 * } piped_foobar_req_t;
 *
 * A TYPICAL RESPONSE:
 *
 * typedef struct {
 *   int type; 
 *   char x;
 *   int y
 * } piped_foobar_resp_t;
 *
 */
#define PIPED_NEW   5
#define PIPED_READ  6
#define PIPED_WRITE 7
#define PIPED_CLOSE 8

typedef struct {
  int type;
  unsigned int tag;
} Pipe_new_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int read_fd;
  int write_fd;
  int ret_val;
} Pipe_new_resp_t;

typedef struct {
  int type;
  unsigned int tag;
  int fd;
  int length;
} Pipe_read_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int length;
  char buf[PIPE_MSG_LEN];
} Pipe_read_resp_t;

typedef struct {
  int type;
  unsigned int tag;
  int fd;
  int length;
  char buf[PIPE_MSG_LEN];
} Pipe_write_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int length;
} Pipe_write_resp_t;

typedef struct { 
  int type;
  unsigned int tag;
  int filedes;
} Pipe_close_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int ret_val;
} Pipe_close_resp_t;

/* The Daemon Message Buffer -- always aligned on a 64 bit boundary */
typedef union { 
  generic_dresp_t dresp;	/* generic response */
  generic_dinit_req_t dinit;	/* init request */
  generic_dping_req_t dping;	/* ping request */
  generic_dquit_req_t dquit;	/* quit request */
  Pipe_new_req_t new_req;
  Pipe_new_resp_t new_resp;
  Pipe_read_req_t read_req;
  Pipe_read_resp_t read_resp;
  Pipe_write_req_t write_req;
  Pipe_write_resp_t write_resp;
  Pipe_close_req_t close_req;
  Pipe_close_resp_t close_resp;
} piped_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

typedef struct reader_data {
  int pid;
  int len;
  unsigned int tag;
} reader_t;

typedef struct pipe_type {
  char    *buf      ;
  int      read_fd  ;
  char    *read_ptr ;
  int      write_fd ;
  char    *write_ptr;
  int      length   ;
  int      wrapped  ;
  lock_t   lock     ;
  queue_t *read_q   ;
} Pipe_t;

/* Deamon Printing Interface -- used to print piped messages */
void piped_print_req_msg(FILE *logp,char *op,piped_msg_t *msgp); /* print req msgs */
void piped_print_resp_msg(FILE *logp,char *op,piped_msg_t *respp); /* print resp msgs */

/* Daemon Communication Interface -- used to communcation with piped */
int piped_init(int dpid); /* one for each message type */
int piped_ping(int dpid);
int piped_quit(int dpid);
int pipe(int filedes[2]);
int pipe_read(void *buf, uint64_t nbytes, int fd);
int pipe_write(void *buf, uint64_t nbytes, int fd);
int pipe_close(int filedes);
