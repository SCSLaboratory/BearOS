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

