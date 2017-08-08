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
 * piped.c -- The piped implementation
 */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <syscall.h>		/* system calls */
#include <msg.h>		/* msgsend/msgrecv */
#include <unistd.h>		/* sleep */
#include <stdint.h>
#include <lock.h>

#include <utils/bool.h>
#include <utils/hash.h>

#include <sbin/piped.h>		/* daemon interface */

/* The piped message buffer */
static piped_msg_t msgbuff;

//#define PIPED_DEBUG 1

hashtable_t *read_htable;
hashtable_t *write_htable;
int pipe_desc_ctr;

static int get_pipe_descriptor(void);
static int do_read(Pipe_read_req_t *req, int pid);
static int do_write(Pipe_write_req_t *req);
static int do_close(Pipe_close_req_t *req);

static void free_pipe(Pipe_t *pipe);

/* fns to search hash table */
static int search_fn(void *ep, const void *skey);
static int read_search_fn(void *ep, const void *skey);
static int write_search_fn(void *ep, const void *skey);

/* 
 * main -- The main loop of the daemon
 *
 * Every message recieves a reply - typically DACK or DNAK
 */
int main(void) {
  Msg_status_t status;		/* status of a recv */
  int done,ready,replysize,rc;
  piped_msg_t *msgp,*replyp;

  Pipe_read_req_t *read_req;
  Pipe_read_resp_t *read_resp;

  Pipe_t *new_pipe, *pipe;

  msgp = replyp = &msgbuff;	/* buffer reused for reply */
  /* 
   * while not done { receive msg, service it, respond to sender } 
   */
  for(done=FALSE, ready=FALSE; !done ; ) {

    /* default reply type is same as the recieved message */
     /* set the default reply size based on generic response */
    replysize=sizeof(generic_dresp_t); 
    /* block and wait to recieve a message */
    msgrecv(ANY,(void*)msgp, sizeof(msgbuff), &status); /* recieve */
    /* service the message and set up the reply */
#ifdef PIPED_DEBUG
    piped_print_req(stdout,"piped-recv",msgp);
#endif 
    switch(msgtype(msgp)) {	/* what message type arrived */
    case DINIT:			/* init */
      read_htable = hopen(MAXOPEN);
      write_htable = hopen(MAXOPEN);

      pipe_desc_ctr = PIPEIDMIN;

      if ( !write_htable || !read_htable )
	ready = FALSE;
      else 
	ready = TRUE;		/* set ready when init complete */
    case DPING:			/* ping */
      respvalue(replyp)=DACK;	/* default reply is ACK */
      if(!ready)		/* send NAK if not ready */
	respvalue(replyp)=DNAK;	
      break;

    case DQUIT:			/* quit for testing only */
      if(ready)	{		/* only quit if ready */
	done=TRUE;		
	respvalue(replyp)=DACK; /* repond with ACK */
      }
      else
	respvalue(replyp)=DNAK; /* respond with NAK */
      break;

    /* request for a new pipe */
    case PIPED_NEW:
      
      /* malloc new pipe */
      new_pipe = (Pipe_t*)malloc(sizeof(Pipe_t));
      if ( !new_pipe ) {
	printf("Could not malloc a new pipe\n");
	exit(EXIT_FAILURE);
      }

      /* malloc the buffer for the pipe */
      new_pipe->buf = (char*)malloc(PIPE_LENGTH * sizeof(char));
      if ( !new_pipe->buf ) {
	printf("Could not allocate the buffer for a new pipe\n");
	exit(EXIT_FAILURE);
      }      

      /* initialize the queue */
      new_pipe->read_q = qopen();
      if ( !new_pipe->read_q ) {
	printf("Could not initialize the read queue\n");
	exit(EXIT_FAILURE);
      }

      /* initialize various members */
      new_pipe->length = 0;
      new_pipe->wrapped = 0;

      /* initialize the lock */
      init_lock(&(new_pipe->lock));
      
      /* read and write pointers both start at beginning of buffer */
      new_pipe->read_ptr = new_pipe->buf;
      new_pipe->write_ptr = new_pipe->buf;
      
      /* get file descriptors for writing and reading */
      new_pipe->read_fd = get_pipe_descriptor();
      new_pipe->write_fd = get_pipe_descriptor();

      /* put pipe in writing hashtable and reading hashtable */
      hput(read_htable, new_pipe, (char*)(&new_pipe->read_fd), sizeof(int));
      hput(write_htable, new_pipe, (char*)(&new_pipe->write_fd), sizeof(int));

      /* build response message */
      ((Pipe_new_resp_t*)replyp)->read_fd = new_pipe->read_fd;
      ((Pipe_new_resp_t*)replyp)->write_fd = new_pipe->write_fd;
      ((Pipe_new_resp_t*)replyp)->ret_val = 0;
      replysize = sizeof(Pipe_new_resp_t);

      break;

    /* request to read a pipe */
    case PIPED_READ:
 
      /* set size of reply message */
      replysize = sizeof(Pipe_read_resp_t);
      
      /* do all the heavy lifting */
      if ( (rc = do_read((Pipe_read_req_t*)msgp, status.src)) < 0 )
	((Pipe_read_resp_t*)replyp)->length = -1;
      else if ( rc == 1 )
	continue; /* dont send response so proc blocks */
      
      break;
      
    /* request to write a pipe */
    case PIPED_WRITE:

      /* set size of reply message */
      replysize = sizeof(Pipe_write_resp_t);

      /* do all the heavy lifting */
      if ( do_write((Pipe_write_req_t*)msgp) < 0 )
	((Pipe_write_resp_t*)replyp)->length = -1;

      break;

    /* request to close a pipe */
    case PIPED_CLOSE:

      /* set size of reply message */
      replysize = sizeof(Pipe_close_resp_t);

      /* TODO: add a file descriptor count so close only really closes the 
               open file desriptor if all open references to it are closed */
      
      /* do the actual closing logic */
      if ( do_close((Pipe_close_req_t*)msgp) < 0)
	((Pipe_close_resp_t*)replyp)->ret_val = -1;

      break;

    default:			/* error! */
      fprintf(stdout,"[error on recv: %d]\n",msgtype(msgp));
      respvalue(replyp)=DNAK;	/* respond with NAK */
      break;
    }
    /* reply to sender using message specific size */
    msgsend(status.src,(void*)replyp, replysize); 
#ifdef PIPED_DEBUG
     piped_print_resp(stdout,"piped-send",replyp);
#endif 
  }
  exit(EXIT_SUCCESS);
}

/* 
   A note on how pipes work:

   they are implemented on as a circular buffer. There is a read pointer 
     and a write pointer. Both point within the buffer and they are 
     incremented on read or write calls. Basically, we are implementing a 
     FIFO with less overhead. Under normal cases, we call the distance 
     between the write ptr and the end of the buffer the "tail"

   Initially:
          |<------------ tail-------------->|
          |_________________________________|
          |                                 |
          |_________________________________|
          ^
          | - read ptr
          | - write ptr
      
   After writing:
                        |<------ tail ----->|
           _____________|___________________|
          |             :                   |
          |_____________:___________________|
          ^             ^
          | - read ptr  | - write ptr

   After reading:
                        |<------ tail ----->|
           _____________|___________________|
          |             :                   |
          |_____________:___________________|
                        ^
                        | - write ptr
                        | - read ptr

   Special case: 
     Conventional wisdom says the read pointer cannot be further "to the right"
       than the write pointer. However, because the buffer is circular, there 
       is a case where this can happen. In the code, we refer to this case as 
       the pipe having "wrapped", and we call the space between the read ptr 
       and the end of the buffer in this case the "tail"

                                    |<-tail->|
           _________________________|________|
          |                         :        |
          |_________________________:________|
            ^                       ^
            | - write ptr           | - read ptr

 */

static int do_read(Pipe_read_req_t *req, int pid) {

  int tail;
  int req_fd;
  int req_len;
  unsigned int req_tag;
  char *resp_buf;
  Pipe_t *pipe;

  Pipe_read_resp_t *resp;

  reader_t *reader;

  /* save request stats because we will be reusing the 
     request buffer for the response */
  req_len = req->length;
  req_fd  = req->fd ;
  req_tag = req->tag;

  /* reuse buffer for response */
  resp = (Pipe_read_resp_t*)req;

  /* find the pipe */
  pipe = hsearch(read_htable, read_search_fn, (char*)(&req_fd), sizeof(int));
  if ( !pipe ) 
    return -1;

  /* aqcuire pipe's lock */
  aquire_lock(&(pipe->lock));

  /* is there enough to read? */
  if ( !pipe->length ) {

    release_lock(&(pipe->lock));

    /* TODO: HOW DO I TAKE DEAD PROCESSES OUT OF THE WAITING QUEUE */
    if ( pid ) {

      /* add proc to reader queue */
      reader = (reader_t*)malloc(sizeof(reader_t));
      if ( !reader ) 
	return -1;
      
      reader->pid = pid;
      reader->len = req_len;
      reader->tag = req_tag;
      
      qput(pipe->read_q, (void*)reader);
      
      return 1; /* don't send response message */
    }
    else
      return -1;
  }

  /* it will definitely succeed now */

  /* update response length and pipe length */
  req_len = ( req_len < pipe->length ? req_len : pipe->length );

  resp->length = req_len;
  pipe->length -= req_len;
  
  /* where we should be writing to */
  resp_buf = resp->buf;

  /* if it is wrapped, handle tail first */
  if ( pipe->wrapped ) {
    
    /* calculate size of tail */
    tail = pipe->buf + PIPE_LENGTH - pipe->read_ptr;

    /* if the read will be bigger than the tail */
    if ( tail <= req_len ) {

      /* read the tail first */
      memcpy(resp_buf, pipe->read_ptr, tail);      

      /* update lengths and pointers */
      req_len -= tail;
      resp_buf += tail;
      pipe->read_ptr = pipe->buf;

      /* we are no longer wrapped */
      pipe->wrapped = 0;
    }
  }

  /* do the read, or rest of read if we already read tail */
  memcpy(resp_buf, pipe->read_ptr, req_len);

  /* move pointer */
  pipe->read_ptr += req_len;
  
  /* release the lock */
  release_lock(&(pipe->lock));

  return 0;
}
      
static int do_write(Pipe_write_req_t *req) {

  int tail;
  int req_fd;
  int req_len;
  char *req_buf;
  Pipe_t *pipe;

  Pipe_write_resp_t *resp;

  piped_msg_t read_msg;

  reader_t *reader;

  /* save request stats because we will be reusing the 
     request buffer for the response */  
  req_len = req->length;
  req_fd  = req->fd ;
  req_buf = req->buf;

  /* reuse buffer for response */
  resp = (Pipe_write_resp_t*)req;

  /* find the pipe */
  pipe = hsearch(write_htable, write_search_fn, (char*)(&req_fd), sizeof(int));
  if ( !pipe ) 
    return -1;

  /* aquire the lock */
  aquire_lock(&(pipe->lock));

  /* is there enough room to write? */
  if ( (PIPE_LENGTH - pipe->length) < req_len ) {
    release_lock(&(pipe->lock));
    return -1; /* TODO: Block? */
  }

  /* we know it is going to succeed */

  /* update lengths */
  resp->length = req_len;
  pipe->length += req_len;

  /* calculate size of tail */
  tail = pipe->buf + PIPE_LENGTH - pipe->write_ptr;
  
  /* if the write will cause the buffer to "wrap" */
  if ( tail <= req_len ) {
    
    /* write the tail first */
    memcpy(pipe->write_ptr, req_buf, tail);
    
    /* update lengths and pointers */
    req_len -= tail;
    req_buf += tail;
    pipe->write_ptr = pipe->buf;

    /* update pipe status */
    pipe->wrapped = 1;
  }
  
  /* do the write, or rest of write if already wrote tail */
  memcpy(pipe->write_ptr, req_buf, req_len);

  /* move pointer */
  pipe->write_ptr += req_len;
  
  /* release lock */
  release_lock(&(pipe->lock));

  /* check to see if someone can read now */
  while ( pipe->length ) {

    reader = (reader_t*)qget(pipe->read_q);
    if ( reader ) { 
      
      /* replicate a request from the reader */
      read_msg.read_req.type   = PIPED_READ;
      read_msg.read_req.length = reader->len;
      read_msg.read_req.fd     = pipe->read_fd;
      read_msg.read_req.tag    = reader->tag;
      
      if ( do_read((Pipe_read_req_t*)(&read_msg), 0) < 0 ) {
	qput(pipe->read_q, reader);
	return 0;
      }
      
      msgsend(reader->pid, (Pipe_read_resp_t*)(&read_msg), 
	      sizeof(Pipe_read_resp_t));
	      free(reader);
    }
    else 
      break;
  }

  return 0;
}

static int do_close(Pipe_close_req_t *req) {
  
  int filedes;
  Pipe_t *pipe;

  /* retrieve the file descriptor */
  filedes = req->filedes;

  /* we dont know if its a write or a read file descriptor... */
  
  if ( pipe = hremove(write_htable, write_search_fn, (char*)(&filedes), sizeof(int)) ) { 
    
    /* it is a write file descriptor */

    /* close write file descriptor */
    pipe->write_fd = -1;
    
    /* is the read side also closed? */
    if ( pipe->read_fd == -1 ) 
      free_pipe(pipe);
  }
  else if ( pipe = hremove(read_htable, read_search_fn, (char*)(&filedes), sizeof(int)) ) {
    
    /* it is a read file descriptor */
    
    /* close the read file descriptor */
    pipe->read_fd = -1;
    
    /* is the write side also closed? */
    if ( pipe->write_fd == -1 )
      free_pipe(pipe);
  }
  else /* bad file descriptor. */
    return -1;

  return 0;
}

/* search for just a read  pipe descriptor */
static int read_search_fn(void *ep, const void *skey) {

  if ( ((Pipe_t*)ep)->read_fd  == (*(int*)skey) )
    return 1;

  return 0;
}

/* search for just a write pipe descriptor */
static int write_search_fn(void *ep, const void *skey) {

  if ( ((Pipe_t*)ep)->write_fd == (*(int*)skey) )
    return 1;

  return 0;
}

/* search for either write or read pipe descriptor */ 
static int search_fn(void *ep, const void *skey) {

  if ( ((Pipe_t*)ep)->write_fd == (*(int*)skey) ||
       ((Pipe_t*)ep)->read_fd  == (*(int*)skey)  )
    return 1;

  return 0;
}

/* search descriptor space for free descriptor */
static int get_pipe_descriptor(void) {

  int save;

  save = pipe_desc_ctr;
  
  while ( hsearch(write_htable, search_fn, (char*)(&pipe_desc_ctr), sizeof(int)) ) {
    if ( pipe_desc_ctr == PIPEIDMAX )
      pipe_desc_ctr = PIPEIDMIN;
    else if ( pipe_desc_ctr == save ) {
      save = -1;
      break;
    }
    else
      pipe_desc_ctr++;
  }

  return (save > 0 ? pipe_desc_ctr++ : -1);
}

static void free_pipe(Pipe_t *pipe) { 

  /* just to be certain */
  if ( pipe->read_fd != -1 )
    hremove(read_htable, read_search_fn, (char*)(&pipe->read_fd), sizeof(int));
  if ( pipe->write_fd != -1 )
    hremove(write_htable, write_search_fn, (char*)(&pipe->write_fd), sizeof(int));

  /* free resources */
  qclose(pipe->read_q);
  free(pipe->buf);
  free(pipe);

  return;
}
