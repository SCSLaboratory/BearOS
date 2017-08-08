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
 * queue.h -- public interface to the queue module
 */

/* the queue representation is hidden from users of the module */
typedef void queue_t;		

/* create an empty queue */
queue_t* qopen(void);        

/* deallocate a queue, frees everything in it */
void qclose(queue_t *qp);   

/* put element at end of queue */
int qput(queue_t *qp, void *elementp); 

/* get first element from queue */
void* qget(queue_t *qp);

/* apply a void function to every element of a queue */
void qapply(queue_t *qp, void (*fn)(void* elementp));

/* apply a void function with an argument to every element of a queue */
void qapply2(queue_t *qp, void *arg, void (*fn)(void *arg, void *elementp));

void *qapply2until(queue_t *qp, void *arg, int (*fn)(void *, void *));

/* search a queue using a supplied boolean function, returns an element */
void* qsearch(queue_t *qp, 
	      int (*searchfn)(void* elementp,const void* keyp),
	      const void* skeyp);

/* search a queue using a supplied boolean function, removes and
 * returns the element 
 */
void* qremove(queue_t *qp,
	      int (*searchfn)(void* elementp,const void* keyp),
	      const void* skeyp);

/* concatenatenates elements of q2 into q1, q2 is dealocated upon completion */
void qconcat(queue_t *q1p, queue_t *q2p);
