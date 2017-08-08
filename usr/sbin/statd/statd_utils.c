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
 * statd_utils.c -- the statd utilities shared by both the daemon
 * and any program that communicates with it. Utilities that are used
 * to communcate with daemon, but not used by the daemon itself are
 * placed in statd_if.c.
 */
#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* uint8_t */

#include <sbin/statd.h>		/* daemon interface */

/* 
 * statd_print_req -- a function that logs every type of request
 * message used by the daemon in a human readable format -- every
 * message used by the daemon must have an entry in this functions
 * switch statement.
 *
 * NOTE: This function is exported for testing purposes and its name
 * is unique to the daemon.
 */
void statd_print_req(FILE *logp,char *op,statd_msg_t *msgp) {

  fprintf(logp,"%s: ",op);
  /* print the message in a readable format */
  switch(msgtype(msgp)) {
  case DINIT:
    fprintf(logp,"[init]\n");	/* init daemon */
    break;
  case DPING:			/* ping */
    fprintf(logp,"[ping]\n");	
    break;
  case DQUIT:			/* quit */
    fprintf(logp,"[quit]\n");	
    break;
  default:			/* unknown */
    fprintf(logp,"[unknown request: %d]\n",msgtype(msgp)); 
    break;
  }
}

/* 
 * statd_print_resp -- a function that logs every type of response
 * message used by the daemon in a human readable format -- every
 * message used by the daemon must have an entry in this functions
 * switch statement.
 *
 * NOTE: This function is exported for testing purposes and its name
 * is unique to the daemon.
 */
void statd_print_resp(FILE *logp,char *op,statd_msg_t *msgp) {

  fprintf(logp,"%s: ",op);
  /* print the message in a readable format */
  switch(msgtype(msgp)) {
  case DINIT:
    fprintf(logp,"[init,");	/* init daemon */
    break;
  case DPING:			/* ping */
    fprintf(logp,"[ping,");	
    break;
  case DQUIT:			/* quit */
    fprintf(logp,"[quit,");	
    break;
  default:			/* unknown */
    fprintf(logp,"[unknown respnse: %d",msgtype(msgp)); 
    break;
  }
  if(respvalue(msgp)==DACK)
    fprintf(logp,"ACK]\n");
  else if(respvalue(msgp)==DNAK)
    fprintf(logp,"NAK\n");
  else
    fprintf(logp,"%d]\n",respvalue(msgp));
}

