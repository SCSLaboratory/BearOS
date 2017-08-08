#pragma once
/*******************************************************************************
 *
 * File: msg.h
 *
 ******************************************************************************/
#include <sbin/syspid.h>

/* STATUS FLAGS */
#define MNONE    0
#define MSEND  0x1
#define MRECV  0x2

/* RETURN VALUES */
#define SUCCESS  0
#define FAILURE -1

typedef struct {
  int src;
  int bytes_rcvd;
  int msgsize_orig;
} Msg_status_t;

typedef struct {
  int direction;    /* MSEND or MRECV */
  int dst;          /* Destination */
  int src;          /* who sent the message... filled in by OS */
  int len;          /* number of bytes contained in buf */
  void *buf;
  Msg_status_t *status;
} Message_t;

/* CAUTION: the order of these elements in this structure is important:
 * The destpid is used to search the hash table and they are both used to
 * decide is a message matches a recieve call.
 */
typedef struct searchstruct {
  int destpid;
  int srcpid;
  unsigned int tag;
} MsgHeader;

unsigned int get_msg_tag();

/* Send a message to another process */
int msgsend(int pid,void* buf,int count);

/* Send a message to another process (Forensics version) */
int msgsend_1(int pid,void* buf,int count);

/* Receive a message from another process */
int msgrecv(int pid,void* buf,int buf_len,Msg_status_t* status);

