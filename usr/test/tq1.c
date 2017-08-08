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
 * tq1.c -- regression test for the queue module
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/queue.h>
#include <utils/tutils.h>

//#define TQ1_DEBUG 1

#define DATASIZE 10		/* the default size of a persons data */

int main(int argc, char *argv[]) {
  queue_t *q1;
  void *p1,*p2,*p3;
  void *ep;
  int ret;

  if(argc!=1) {
    printf("Usage: tq1\n");
    exit(EXIT_FAILURE);
  }
  /* make some generic people to be in the queue */
  p1 = make_person("steve",10,DATASIZE);
  p2 = make_person("bill",11,DATASIZE);
  p3 = make_person("john",12,DATASIZE);
  
  /* open a queue */
  q1=qopen();

  /* try to get on an empty queue */
  if((ep=qget(q1)) != NULL) 
    return(EXIT_FAILURE);

  /* put 3 people in it */
  qput(q1,p1);
  qput(q1,p2);
  qput(q1,p3);

#ifdef TQ1_DEBUG  
  printf("\nQueue:\n");
  qapply(q1,print_person);
#endif

  /* get them, check them, and free them */
  ep=qget(q1);
  if(!check_person(ep,"steve",10,DATASIZE))
    return(EXIT_FAILURE);
  free_person(ep);
  ep=qget(q1);
  if(!check_person(ep,"bill",11,DATASIZE))
    return(EXIT_FAILURE);
  free_person(ep);
  ep=qget(q1);
  if(!check_person(ep,"john",12,DATASIZE))
    return(EXIT_FAILURE);
  free_person(ep);

  /* make sure the queue is empty */
  if((ep=qget(q1)) != NULL) 
    return(EXIT_FAILURE);

  /* close the queue up */
  qclose(q1);
  return(EXIT_SUCCESS);
}

