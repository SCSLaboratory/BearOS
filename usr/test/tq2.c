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
 * tq2.c -- regression test for the queue module
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/queue.h>
#include <utils/tutils.h>

/* define DEBUGTEST to output debugging */
//#define TQ2_DEBUG 1

/*
 * main -- uses a queue and exercizes open/put/get/close
 */
int main(int argc, char *argv[]) {
  queue_t *q1;
  void *p1;
  void *ep;
  int i,n,datasize;

  if(argc!=3 || ((n=atoi(argv[1]))<=0) || ((datasize=atoi(argv[2]))<=0)) {
    printf("Usage: tq2 <queue_entries> <entrysize>\n");
    exit(EXIT_FAILURE);
  }

  /* open a queue */
  q1=qopen();

  /* get from empty queue */
  if((ep=qget(q1)) != NULL) 
    return(EXIT_FAILURE);

  /* fill the queue */
  for(i=0;i<n; i++) {
    p1 = make_person("steve",i,datasize);
#ifdef TQ2_DEBUG
    print_person(p1);
#endif
    qput(q1,(void*)p1);
  }

  /* empty the queue */
  for(i=0;i<n; i++) {
    ep=qget(q1);
    if(!check_person(ep,"steve",i,datasize)) {
#ifdef TQ2_DEBUG
      print_person(ep);
#endif
      return(EXIT_FAILURE);
    }
    free(ep);
  }

  /* make sure the queue is empty */
  if((ep=qget(q1)) != NULL) 
    return(EXIT_FAILURE);

  /* close the queue up */
  qclose(q1);
  return(EXIT_SUCCESS);
}

