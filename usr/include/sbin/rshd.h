#pragma once
/*
 * rshd.h -- The generic rshd interface 
 */
#include <utils/bool.h>
#include <sbin/daemon_msg_types.h> /* msgtype macro */
#include <sbin/kbd.h>		/* message formats for the keyboard */
#include <sbin/vgad.h> 		/* message formats for the vga */

#define RSH_PORT 1604

/* The Daemon Message Buffer -- always aligned on a 64 bit boundary */
typedef union { 
  Getstr_req_t dstrreq;		/* req a string from stdin */
  Getstr_resp_t dstrresp;	/* responce from stdin */
  Vgad_msg_t vgamsg;		/* msgs to vgad */
} rshd_msg_t __attribute__ ((aligned(sizeof(uint64_t))));




