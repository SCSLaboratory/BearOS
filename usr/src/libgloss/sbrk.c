//#include <_syslist.h>
#include <_ansi.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>

static int umalloc( int bytes );
// extern char   sbrk_end; /* Set by linker.  */
static uint64_t heap_num_pages;
static char *heap_end;
static char *heap_start;

void *
_DEFUN (sbrk ,(incr),
        int incr)
{

  char *prev_heap_end; 

  if (heap_end == 0) {
    (void)umalloc(0); 
    heap_start = heap_end;
  }

  /* Check to see if it is necessary to call umalloc() */
  if( ((uint64_t)(heap_end + incr - heap_start)) >= (heap_num_pages*4096))
    if(umalloc(incr) == EXIT_FAILURE) 
      return (void *)heap_end;
	
  /* Set the heap end accordingly - shrink/increase */
  prev_heap_end = heap_end; 
  heap_end += incr; 

  /* return whichever is less, accounting for -incr and positive incr */
  return (void *) (heap_end < prev_heap_end ? heap_end : prev_heap_end);

}

static int umalloc(int bytes) {
  Umalloc_req_t req;
  Umalloc_resp_t resp;
  Msg_status_t status;
  int size;

  req.type = SC_UMALLOC;
  req.bytes = bytes;

  msgsend(SYS, &req, sizeof(Umalloc_req_t));
  msgrecv(SYS, &resp, sizeof(Umalloc_resp_t), &status);

  /* brk? */
  if(bytes == 0) {
    heap_end = (char*)resp.addr;
    return 0;
  }

  size = bytes;
	
  /* Add the number of pages allocated to heap_num_pages */
  if(resp.ret_val == EXIT_SUCCESS) {
    if(size % 4096)
      size += 4096 - (size % 4096);
  }
  heap_num_pages += (size/4096);

  return resp.ret_val;
} 
