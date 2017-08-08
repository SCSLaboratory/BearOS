/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#pragma once
/*
* kmsg.h
*/

#include <sbin/syspid.h>  /* PID constants */
#include <msg.h>
#include <proc.h>

/*********************************************************************
 **************************** DEFINES ********************************
 *********************************************************************/

/* The interrupt vector used by the syscall interface. */
#define SYSCALL_VECTOR 0x80

/* STATUS FLAGS */
#define MNONE    0
#define MSEND  0x1
#define MRECV  0x2

/* RETURN VALUES */
#define SUCCESS  0
#define FAILURE -1

/* These go to/from the net task. */
/**** DEFINITIONS ****/
/* To net from driver */
#define NET_IFADD         1
#define NET_IFLINK        2
#define NET_PKT_INCOMING  3
#define NET_TIMER_MSG     4
/* To net from user procs */
#define NET_IFCONFIG      5
#define NET_SOCKET        6
#define NET_BIND          7
#define NET_LISTEN        8
#define NET_CONNECT       9
#define NET_ACCEPT       10
#define NET_SEND         11
#define NET_SENDTO       12
#define NET_RECV         13
#define NET_RECVFROM     14
#define NET_GHBN         15
#define NET_CLOSE        16
#define NET_UPDATE      17

/* These go to/from a network driver (for us, bced). */
/**** DEFINITIONS ****/
/* These go to the driver. */
#define NDRV_INIT      1  /* Init network card      */
#define NDRV_OUTPKT    2  /* Outgoing packet        */
#define NDRV_TIMER_MSG 3  /* Timer msg              */
#define NDRV_CONFIG    4  /* Config msg from kernel */

/*******************************************************************************
 ***************************** MESSAGE FORMAT **********************************
 ******************************************************************************/

/* Public functions */
int  kmsg_init(void);
void kmsg_send(Message_t*);
int  kmsg_recv(Proc_t*,Message_t*,int);
int  kmsg_purge(Proc_t*);
void kmsg_handle_syscall(int, Message_t*);
void kmsg_ps(void *rp);
