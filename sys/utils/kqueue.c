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
 * queue.c -- implements a generic queue
 * Hides the internal representation of a queue.
 *
 * If you edit this file please do not screw up the emacs tabbing !!! (ST)
 */
#include <kmalloc.h>
#include <kqueue.h>
#include <constants.h>

/* general definitions */
#define DEFAULT_MAX_FREE 20	/* at most 20 free links allowed */
#define qlock()
#define qunlock()


/* BEGINNING OF PRIVATE SECTION */
/* 
 * A hidden queue (hqueue_t) is a linked list of hidden links
 * (hlink_t) accessed through front and back pointers -- these
 * structures are not visible outside the queue module.
 */
typedef struct link_struct {
  struct link_struct *linknextp;           /* next Link */
  struct link_struct *linkprevp;           /* previous Link */
  void *linkelementp;                      /* ptr to queue element */
} hlink_t;				   /* a hidden link */

/* link accessor macros */
#define next(l) (((hlink_t*)l)->linknextp)
#define prev(l) (((hlink_t*)l)->linkprevp)
#define element(l) (((hlink_t*)l)->linkelementp)

typedef struct {
  hlink_t *queuefrontp;		/* pointer to front of queue */
  hlink_t *queuebackp;		/* pointer to end of queue */
  hlink_t *queuefreep;		/* free list of links */
  int freespaces;		/* number of spaces for free links */
} hqueue_t;			/* a hidden queue */

/* queue accessor macros */
#define back(q) (((hqueue_t*)q)->queuebackp)
#define front(q) (((hqueue_t*)q)->queuefrontp)
#define qfree(q) (((hqueue_t*)q)->queuefreep)
#define spaces(q) (((hqueue_t*)q)->freespaces)

/* 
 * hidden helper functions 
 */
static void free_link(hqueue_t *qp,hlink_t *lp) {		       
  //  if(spaces(qp)>0) {		/* room on the freelist */
  //    next(lp) = qfree(qp);	/* add link to freelist */
  //    qfree(qp) = lp;				
  //    spaces(qp)--;		/* one less space for a free link */
  //  }						
  //  else				/* otherwise, just free it */
    kfree_track(KQUEUE_SITE,(void*)lp);		
}

static hlink_t* get_link(hqueue_t *qp) {
  hlink_t* lp;
  
  if(qfree(qp) != NULL) {	/* any links on freelist */
    lp=qfree(qp);		/* grab one */
    qfree(qp) = next(lp);
    spaces(qp)++;		/* one more space for a free link */
  }
  else 				/* otherwise, malloc one */
    lp = (hlink_t*)kmalloc_track(KQUEUE_SITE,sizeof(hlink_t));
  if (lp) {			/* then initialize it */
    next(lp) = NULL; 
    prev(lp) = NULL;
    element(lp) = NULL;
  }
  return lp;                               
}
/* END OF PRIVATE SECTION */



/* BEGINNING OF PUBLIC SECTION */

/* 
 * qopen -- opens a queue 
 */
queue_t *qopen(void) {
  hqueue_t* qp;
  void *rp;
  
  qlock();
  qp = (hqueue_t*)kmalloc_track(KQUEUE_SITE,sizeof(hqueue_t));
  if(qp) {			/* intialize the queue to empty */
    front(qp) = NULL;
    back(qp) = NULL;  
    qfree(qp) = NULL;
    spaces(qp) = DEFAULT_MAX_FREE; /* number of free links allowed */
  }
  rp=(void*)qp;
  qunlock();
  return (queue_t*)rp;
}

/* 
 * qclose -- close a queue by free'ing each link in the queue, its
 * associated elements, and the free list.
 */
void qclose(queue_t *qp) {
  hlink_t *p, *holdp;
  
  qlock();
  for(p=front(qp); p!=NULL; ) { /* p points to links */
    holdp=p;			/* save the current link */
    if(element(p)!=NULL)	/* if the element exists */
      kfree_track(KQUEUE_SITE,element(p));		/* free it */
    p=next(p);			/* move p on to next link */
    kfree_track(KQUEUE_SITE,holdp);		/* free the current link */
  }
  for(p=qfree(qp); p!=NULL; ) {	/* p points to an empty link */
    holdp=p;			/* hold the current link */
    p=next(p);			/* move p on to next link */
    kfree_track(KQUEUE_SITE,holdp);		/* free the current link */
  }
  kfree_track(KQUEUE_SITE,(void*)qp);		/* free the queue structure */
  qunlock();
}

/*
 * qadd -- adds an value ep to the back of queue qp
 */
int qput(queue_t *qp, void *ep) {
  hlink_t *newp,*bp;
  int rc;
  
  qlock();
  newp=get_link(qp);                       /* get a new link */
  if(newp) {                               /* next already null */
    element(newp) = ep;		/* add queue element to link */
    bp=back(qp);
    if(bp) {			    /* list is not empty */
      next(bp)=newp;                /* change last entry */
      prev(newp)=bp;                /* previous is old back */
    }
    else
      front(qp) = newp;             /* front of the queue */
    back(qp) = newp;		    /* new is now queue back */
  } 
  if (newp == NULL)
    rc = -1;
  else
    rc = 0;
  qunlock();
  return rc;
}


void* qget(queue_t *qp) {
  hlink_t *fp;
  void* ep;
  
  qlock();
  fp=front(qp);                        /* find front of queue */
  if(fp) {                             /* queue not empty */
    ep = element(fp);                  /* take the first value */
    front(qp) = next(fp);	/* new front is next in queue */
    if(front(qp)==NULL)		/* if list now empty */
      back(qp)=NULL;		/* back also null */
    else
      prev(front(qp)) = NULL;	     /* no prev for new front */
    free_link(qp,fp);                /* add link to free list */
  }
  else				/* nothing in queue */
    ep = NULL;			/* nothing to return */
  qunlock();
  return ep;
}

/*
 * qapply -- applies a function to every element of the queue 
 */
void qapply(queue_t *qp, void (*fn)(void* ep)) {
  hlink_t *p;
  
  qlock();
  for(p=front(qp); p!=NULL ; p=next(p)) {
    (*fn)(element(p));                     /* apply fn to all  */
  }
  qunlock();
}

void qapply2(queue_t *qp, void *arg, void (*fn)(void *arg, void *ep)) {
  hlink_t *p;
  
  qlock();
  for(p=front(qp); p!=NULL; p=next(p)) {
    (*fn)(arg, element(p));              /* apply fn to all */
  }
  qunlock();
}

void *qapply2until(queue_t *qp, void *arg, int (*fn)(void *, void *)) {
  hlink_t *p;
  
  qlock();
  for(p=front(qp); p!=NULL; p=next(p)) {
    if((*fn)(arg, element(p)) == 1) {
      qunlock();
      return element(p);
    }
  }
  qunlock();
  return NULL;
}

/* 
 * qsearch -- applies a search function to every element of queue
 * and returns a pointer to the element if its is found
 */
void* qsearch(queue_t *qp, 
	      int (*searchfn)(void* elementp, const void* searchkey),
	      const void* keyp) {
  hlink_t *p;
  void *result;
  int found;
  
  qlock();
  result=NULL;
  for(found=FALSE, p=front(qp) ; 
      p!=NULL && !(found=(*searchfn)(element(p),keyp)) ;
      p=next(p))
    ;                                    /* apply fn to all  */
  if(found) 
    result=element(p);
  qunlock();
  return result;
}

/*
 * qremove -- applies a search function to every element of the queue;
 * removes and returns the sought for element if it is found
 */
void* qremove(queue_t *qp, 
	      int (*searchfn)(void* elementp,const void* searchkey),
	      const void* keyp) {
  hlink_t *p;
  void *result;
  int found;
  
  qlock();
  result=NULL;			/* no result by default */
  for(found=FALSE, p=front(qp) ; 
      p!=NULL && !(found=(*searchfn)(element(p),keyp)) ;
      p=next(p))
    ;				/* apply fn to all  */
  if(found) {
    result=element(p);
    if(prev(p))			       /* something before p? */
      next(prev(p)) = next(p);         /* prev points past p */
    else			       /* p is front of q */
      front(qp) = next(p);             /* set new front */
    if(next(p))			       /* something after p? */
      prev(next(p)) = prev(p);         /* next points past p */
    else			       /* p is back of q */
      back(qp) = prev(p);              /* set new back */
    free_link(qp,p);		       /* add link to free list */
  }
  qunlock();
  return result;
}

/*
 * qconcat -- concatenate q2 into q1 -- q2 is no longer valid after
 * this operation 
 */
void qconcat(queue_t *q1p, queue_t *q2p) {
  hlink_t *p,*holdp;

  qlock();
  if(front(q2p)!=NULL) {
    if(front(q1p)==NULL) {	/* q1 empty, q2 is result */
      front(q1p) = front(q2p);
      back(q1p) = back(q2p);
      /* leave the free list & spaces alone */
    } 
    else {	
      /* put q2 at back of q1 */
      prev(front(q2p)) = back(q1p);        /* q2 front pts back at q1 */
      next(back(q1p)) = front(q2p);
      back(q1p) = back(q2p);
      /* leave the free list and spaces alone */
    }
  }
  /* deallocate the free links in q2 */
  for(p=qfree(q2p); p!=NULL; ) { /* p points to an empty link */
    holdp=p;			 /* hold the current link */
    p=next(p);			 /* move p on to next link */
    free_link(q1p,holdp);	 /* free the current link */
  }
  kfree_track(KQUEUE_SITE,q2p);                              /* deallocate q2 */
  qunlock();
}

/* manipulation of the number of free links kept */
void qsetfreespaces(queue_t *qp, int cnt) {
  qlock();
  spaces(qp) = cnt;
  qunlock();
}

int qgetfreespaces(queue_t *qp) {
  int rv;
  qlock();
  rv = spaces(qp);
  qunlock();
  return rv;
}
