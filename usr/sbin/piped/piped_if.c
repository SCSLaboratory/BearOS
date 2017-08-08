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
 * piped_if.c -- this file contains the implementation for 
 * functions that communicate with piped. The prototypes are
 * available throught the interface piped.h.
 */
#include <stdio.h>		/* fprintf */
#include <string.h>
#include <stdint.h>		/* uint8_t */
#include <sbin/piped.h>		/* daemon interface */

//define PIPED_IF_DEBUG 1 	/* set this to see messages in test*/

static piped_msg_t msgbuff;
static const piped_msg_t *msgbuffp = &msgbuff;

#ifdef PIPED_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes);
#endif

/* helper function */
static int send_recv_generic(int dpid,int msgsize);

/*
 * The generic interface provides init/ping/quit messages
 */
int piped_init(int dpid) {
  generic_dinit_req_t *msgp;

  msgp = (generic_dinit_req_t*)msgbuffp;  
  msgtype(msgp) = DINIT;
  return send_recv_generic(dpid,sizeof(generic_dinit_req_t));
}

int piped_ping(int dpid) {
  generic_dping_req_t *msgp;

  msgp = (generic_dping_req_t*)msgbuffp;  
  msgtype(msgp) = DPING;
  return send_recv_generic(dpid,sizeof(generic_dping_req_t));
}

int piped_quit(int dpid) {
  generic_dquit_req_t *msgp;

  msgp = (generic_dquit_req_t*)msgbuffp;  
  msgtype(msgp) = DQUIT;
  return send_recv_generic(dpid,sizeof(generic_dquit_req_t));
}

/* function to open a new pipe by sending a request to the pipe daemon */
int pipe(int filedes[2]) {

  Pipe_new_req_t req;
  Pipe_new_resp_t resp;

  Msg_status_t status;

  req.type = resp.type = PIPED_NEW;
  req.tag = resp.tag = get_msg_tag();

  msgsend(PIPED, &req, sizeof(Pipe_new_req_t));
  msgrecv(PIPED, &resp, sizeof(Pipe_new_resp_t), &status);

  if ( resp.ret_val < 0 )
    return -1;
  
  /* return pipe file descriptors */
  filedes[0] = resp.read_fd;
  filedes[1] = resp.write_fd;
  
  return resp.ret_val;
}

/* called by read() if it finds the fd belongs to a pipe */
int pipe_read(void *buf, uint64_t nbytes, int fd) {

  Pipe_read_req_t req;
  Pipe_read_resp_t resp;

  Msg_status_t status;
  
  int len = 0;

  req.type = resp.type = PIPED_READ;
  req.tag = resp.tag = get_msg_tag();
  
  req.fd = fd;

  /* only PIPE_MSG_LEN can be recieved from the pipe daemon 
   *   at a time, so we might have to do more than one call */
  while ( len < nbytes ) {
    req.length = ( nbytes-len > PIPE_MSG_LEN ? PIPE_MSG_LEN : nbytes - len );
    msgsend(PIPED, &req, sizeof(Pipe_read_req_t));

    msgrecv(PIPED, &resp, sizeof(Pipe_read_resp_t), &status);
    if ( resp.length < 0 )
      return -1;

    memcpy(buf, resp.buf, resp.length);

    len += resp.length;
    buf += resp.length;
    
    if ( resp.length < req.length )
      break;
  }

  return len;  
}

/* called by close() if it finds that the file descriptor belongs to a pipe */
int pipe_close(int filedes) {
  
  Pipe_close_req_t req;
  Pipe_close_resp_t resp;

  Msg_status_t status;

  req.type = resp.type = PIPED_CLOSE;
  req.tag = resp.tag = get_msg_tag();

  req.filedes = filedes;

  msgsend(PIPED, &req, sizeof(Pipe_close_req_t));
  msgrecv(PIPED, &resp, sizeof(Pipe_close_resp_t), &status);

  return resp.ret_val;
}

/* called by write() if it finds that the file descriptor belongs to a pipe */
int pipe_write(void *buf, uint64_t nbytes, int fd) {

  Pipe_write_req_t req;
  Pipe_write_resp_t resp;

  Msg_status_t status;
  
  int len = 0;

  req.type = resp.type = PIPED_WRITE;
  req.tag = resp.tag = get_msg_tag();
  
  req.fd = fd;

  /* only PIPE_MSG_LEN can be sent to the pipe daemon 
   *   at a time, so we might have to do more than one call */
  while ( len < nbytes ) {
    req.length = ( nbytes-len > PIPE_MSG_LEN ? PIPE_MSG_LEN : nbytes - len );
    memcpy(req.buf, buf, req.length);
    len += req.length;
    buf += req.length;

    msgsend(PIPED, &req, sizeof(Pipe_write_req_t));

    msgrecv(PIPED, &resp, sizeof(Pipe_write_resp_t), &status);
    if ( resp.length < 0 )
      return -1;
  }

  return 0;  
}

/*
 * send_recv_generic -- assumes a message has been set up to send in
 * the msgbuff, sends it, then waits for a generic response and
 * returns the value associated with it.
 */
static int send_recv_generic(int dpid,int msgsize) {
  Msg_status_t status;
  generic_dresp_t *replyp;

  /* send to piped */
  msgsend(dpid,(void*)msgbuffp,msgsize);
#ifdef PIPED_IF_DEBUG
  piped_print_req(stdout,"generic-send",msgbuffp); /* print msg */
#endif

#ifdef PIPED_IF_DEBUG
  clear_buff((uint8_t*)msgbuffp,sizeof(generic_dresp_t));
#endif

  /* recv from piped */
  replyp = (generic_dresp_t*)msgbuffp;
  msgrecv(dpid,(void*)replyp,sizeof(generic_dresp_t),&status);
#ifdef PIPED_IF_DEBUG
  piped_print_resp(stdout,"generic-recv",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}

#ifdef PIPED_IF_DEBUG
/* A message buffer is used for both sending and recieving. As a
 * result, when debugging, its good practice to clear the buffer to
 * between these operations to ensure that no spurious data is used in
 * communication.
 */
static clear_buff(uint8_t *p,uint64_t numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *p = 0;
}
#endif 

