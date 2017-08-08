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
/*
 * msg.c -- the message passing interface
 */

#include <swint.h>
#include <msg.h>

/* TODO revise this */
#ifndef NULL
#define NULL ((void *)0)
#endif

static unsigned int tag_ctr;

unsigned int get_msg_tag(void) {

  if ( !tag_ctr )
    tag_ctr++;

  return tag_ctr++;
}


/* Send a message to another process */
int msgsend(int dst, void *buf, int buflen) {

  Message_t m;

  m.direction = MSEND;
  m.dst = dst;
  m.len = buflen;
  m.buf = buf;
  m.status = NULL;


  /* not ring0 proc */
  return swint(MSEND,&m);
}

/* Receive a message from another process */
int msgrecv(int src, void *buf, int buflen, Msg_status_t *status) {

  Message_t m;

  m.direction = MRECV;
  m.src    = src;
  m.len    = buflen;
  m.buf    = buf;
  m.status = status;


  /* else (not ring0 proc */
  return swint(MRECV,&m);
}
