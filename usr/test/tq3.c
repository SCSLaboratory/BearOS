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
 * tq3.c -- regression test for the queue module
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/queue.h>
#include <utils/tutils.h>

//#define TQ3_DEBUG 1

#define DATASIZE 10		/* the default size of a persons data */

/* Here is a main test program that uses all functions in 
 * queue interface, making sure to operate on a first, 
 * middle, and last entry whenever appropriate.
 */
int main(int argc, char *argv[]) {
  void *pp;
  int age;
  queue_t *q1,*q2;

  if(argc!=1) {
    printf("Usage: tq3\n");
    exit(EXIT_FAILURE);
  }

  /* make some generic people to be in the queue */
  void *p1 = make_person("steve", 10, DATASIZE);
  void *p2 = make_person("bill", 11, DATASIZE);
  void *p3 = make_person("john", 12, DATASIZE);

  void *p4 = make_person("fred", 13, DATASIZE);
  void *p5 = make_person("george", 14, DATASIZE);
  void *p6 = make_person("nigel", 15, DATASIZE);
  
  /* open a queue, and put 3 people in it */
  q1=qopen();
  qput(q1,p1);
  qput(q1,p2);
  qput(q1,p3);
#ifdef TQ3_DEBUG
  printf("\nqueue 1:\n");
  qapply(q1,print_person);
#endif

  /* open a second queue, and put 3 people in it */
  q2=qopen();
  qput(q2,p4);
  qput(q2,p5);
  qput(q2,p6);
#ifdef TQ3_DEBUG
  printf("\nqueue 2:\n");
  qapply(q2,print_person);
#endif

  /* test the concatenation function */
  qconcat(q1,q2);

#ifdef TQ3_DEBUG
  printf("\nConcatenated queue:\n");
  qapply(q1,print_person);
#endif

  /* When searching, you need:
   *	 -- a queue to search
   *	 -- a function that returns true or false 
   *	 -- a key to search for (passed as a void*)
   * The return value is the element found (as a void*), note
   * that it remains in the queue.
   *
   * Removing works the same way, but the element is removed 
   * from the queue.
   */
  age=10;
  pp=qsearch(q1,is_age,(void*)&age);
  check_person(pp,"steve",10,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nSearching queue for first element (age 10):\n");
  print_person(pp);
#endif
  
  age=13;
  pp=qsearch(q1,is_age,(void*)&age);
  check_person(pp,"fred",13,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nSearching queue for middle element (age 13):\n");
  print_person(pp);
#endif
  
  age=15;
  pp=qsearch(q1,is_age,(void*)&age);
  check_person(pp,"nigel",15,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nSearching queue for last element (age 15):\n");
  print_person(pp);
#endif
  
  age=50;
  pp=qsearch(q1,is_age,(void*)&age);
  if(pp!=NULL)
    return(EXIT_FAILURE);
#ifdef TQ3_DEBUG
  printf("\nSearching for element not present (age 50):\n");
  print_person(pp);
#endif

#ifdef TQ3_DEBUG  
  printf("\nUnchanged queue:\n");
  qapply(q1,print_person);
#endif
  
  age=50;
  pp=qremove(q1,is_age,(void*)&age);
  if(pp!=NULL)
    return(EXIT_FAILURE);
#ifdef TQ3_DEBUG
  printf("\nRemoveing for element not present (age 50):\n");
  print_person(pp);
#endif
  
  age=10;
  pp=qremove(q1,is_age,(void*)&age);
  check_person(pp,"steve",10,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nRemoveing queue for first element (age 10):\n");
  print_person(pp);
#endif
  
  age=13;
  pp=qremove(q1,is_age,(void*)&age);
  check_person(pp,"fred",13,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nRemoveing queue for middle element (age 13):\n");
  print_person(pp);
#endif
  
  age=15;
  pp=qremove(q1,is_age,(void*)&age);
  check_person(pp,"nigel",15,DATASIZE);
#ifdef TQ3_DEBUG
  printf("\nRemoveing queue for last element (age 15):\n");
  print_person(pp);
#endif

#ifdef TQ3_DEBUG
  printf("\nRemaining queue:\n");
  qapply(q1,print_person);
  printf("\nRemoving remaining elements one at a time:\n");
#endif    
  do {
    pp = (void*)qget(q1);
    if(pp!=NULL) {
#ifdef TQ3_DEBUG
      printf("\nRemoved: ");
      print_person(pp);
      printf("Remainder:\n");
      qapply(q1,print_person);
#endif
    }
#ifdef TQ3_DEBUG
    else 
      printf("Empty Queue\n");
#endif
  } while(pp!=NULL);
  
  /* free all elements */
  qapply(q1,free_person);
  /* close the list */
  qclose(q1);
  return(EXIT_SUCCESS);
}

