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
 * tq4.c -- regression test for the queue module
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/queue.h>
#include <utils/tutils.h>

//#define TQ4_DEBUG 1

/* number of entries to put in/get out of the queue at a shot */
#define ENTRIES 10

#define DATASIZE 10		/* the default size of a persons data */

/* 
 * main program -- puts and gets from a queue to test the free list 
 */
int main(int argc, char *argv[]) {
  queue_t *qp;
  void *pp;
  void *ep;
  int i,ret;

  if(argc!=1) {
    printf("Usage: tq4\n");
    exit(EXIT_FAILURE);
  }

  /* open a queue */
  qp=qopen();
  /* set max number of spaces for free links to be 5 */ 
  qsetfreespaces(qp,5);

  /* put 10 people in the queue */
#ifdef TQ4_DEBUG
  printf("\nput %d entries, expect 5 spaces (%d found):\n",
	 ENTRIES,qgetfreespaces(qp));
#endif
  for(i=1; i<=ENTRIES; i++) {
    pp = make_person("nm",i,DATASIZE);
    qput(qp,pp);
#ifdef TQ4_DEBUG
    print_person(pp);
    printf("spaces=%d\n",qgetfreespaces(qp));
#endif
  }
  if(qgetfreespaces(qp)!=5)  {
#ifdef TQ4_DEBUG
    printf("ERROR: no links generated, expected 5 spaces, found %d\n",
	   qgetfreespaces(qp));
#endif
    exit(EXIT_FAILURE);
  }

  /* get 10 out generating 5 free links, reducing spaces to 0 */
#ifdef TQ4_DEBUG
  printf("\nget %d entries, expecting 5 spaces initially (%d found)\n",
	 ENTRIES,qgetfreespaces(qp));
#endif
  for(i=1; i<=ENTRIES; i++) {
    ep=qget(qp);
#ifdef TQ4_DEBUG
    print_person(ep);
    printf("spaces=%d\n",qgetfreespaces(qp));
#endif
    free(ep);
  }
  if(qgetfreespaces(qp)!=0) {
#ifdef TQ4_DEBUG
    printf("ERROR: generated 5 links, expected 0 spaces, found %d\n",
	   qgetfreespaces(qp));
#endif
    exit(EXIT_FAILURE);
  }

  /* put 10 more people in the queue, using up the 5 free links available */
#ifdef TQ4_DEBUG
  printf("\nput %d entries using 5 links, initially spaces=%d (0 expected):\n",
	 ENTRIES,qgetfreespaces(qp));
#endif
  for(i=1; i<=ENTRIES; i++) {
    pp = make_person("nm",i,DATASIZE);
    qput(qp,pp);
#ifdef TQ4_DEBUG
    print_person(pp);
    printf("spaces=%d\n",qgetfreespaces(qp));
#endif
  }
  if(qgetfreespaces(qp)!=5) {
#ifdef TQ4_DEBUG
    printf("ERROR: expected 5 spaces: %d found\n",qgetfreespaces(qp));
#endif
    exit(EXIT_FAILURE);
  }

  /* get 10 out generating 5 free links, and no spaces */
#ifdef TQ4_DEBUG
  printf("\nget %d entries, spaces=%d (5 expected)\n",
	 ENTRIES,qgetfreespaces(qp));
#endif
  for(i=1; i<=ENTRIES; i++) {
    ep=qget(qp);
#ifdef TQ4_DEBUG
    print_person(ep);
    printf("spaces=%d\n",qgetfreespaces(qp));
#endif
    free(ep);
  }
  if(qgetfreespaces(qp)!=0) {
#ifdef TQ4_DEBUG
    printf("ERROR: expected 0 spaces: %d found\n",qgetfreespaces(qp));
#endif
    exit(EXIT_FAILURE);
  }

  /* if we got here close up the queue and succeed */
  qclose(qp);
  exit(EXIT_SUCCESS);
}
