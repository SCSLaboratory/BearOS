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
 * shash.c -- implements a generic hash table as an indexed set of queues.
 */
#include <stdlib.h>
#include <stdint.h>
#include <utils/queue.h>
#include <utils/shash.h>

#define hlock()
#define hunlock()


/* PRIVATE SECTION */

/* the hidden structre of a hash table structure */
typedef struct {
  int table_size;		/* the size of the table */
  void **table;			/* pointer to a table of void* */
} hhash_t;

/* accessor macros */
#define hsize(htp) (((hhash_t*)htp)->table_size)
#define htable(htp) (((hhash_t*)htp)->table)
#define hqueue(htp,qindex) (*(htable(htp)+qindex))

/* 
 * The Hash Function
 * prototype: int hashfn(hhash_t *ctx,int key); 
 * map negative keys to index 0; otherwise uses modulo tablesize
 */
#define hashfn(ctx, key) ( (key) < 0 ? (0) : (key % (hsize(ctx))))
//#define hashfn(ctx,key) ( (key) % (hsize(ctx)) )


static void count_element(void *arg, void *ep){
	 int *count = (int32_t*)arg;
	if(ep!=NULL)
		*count = *count + 1;
} 


/* END OF PRIVATE SECTION */



/* PUBLIC SECTION */




shashtable_t *shopen(uint32_t hsize) {
  hhash_t *htp;
  void **tp, **p, **endp;
  
  hlock();
  htp = malloc(sizeof(hhash_t));	  /* the hash table */
  tp = malloc(sizeof(void*)*hsize);       /* indexed set of queues */
  hsize(htp) = hsize;			  /* set the size */
  htable(htp) = tp;			  
  for(p=tp, endp=tp+hsize; p<endp; p++)
    *p=qopen();			/* each entry is a queue */
  hunlock();
  return (shashtable_t*)htp;
}

void shclose(shashtable_t *htp) {
  void **tp,**p,**endp;
  
  hlock();
  tp = htable(htp);
  for(p=tp, endp=tp+hsize(htp); p<endp; p++)
    qclose(*p);				  /* close each queue */
  free(tp);                               /* free the index */
  free(htp);                              /* free the hash table */
  hunlock();
}

/*
 * hput -- adds an value to a hash table under a specific key
 */
void shput(shashtable_t *htp, void *ep, int key) {
  int index;
  void *qp;
  
  hlock();
  index=hashfn(htp, key);	/* get queue index */
  qp=hqueue(htp, index);	/* find queue */
  qput(qp, ep);			/* put in queue */
  hunlock();
}

/*
 * shget_size -- get the current number of elements or size of the table 
 */
int shget_size(shashtable_t *htp){
	int count;
	shapply2(htp, count_element, &count);
	return count;
}
		
/*
 * happly2 -- apply a function to every entry in the table
 */
void shapply2(shashtable_t *htp, void (*fn)(void *arg, void *ep), int *arg) {
  int i,hsize;
  void *qp;
  
  hlock();
  hsize = hsize(htp);
  for(i=0; i<hsize; i++) {	  /* for each entry in table */
    if((qp=hqueue(htp, i))!=NULL) /* take queue i */
      qapply2(qp, arg, fn);		  /* apply in queue */
  }
  hunlock();
}

/*
 * happly -- apply a function to every entry in the table
 */
void shapply(shashtable_t *htp, void (*fn)(void *ep)) {
  int i,hsize;
  void *qp;
  
  hlock();
  hsize = hsize(htp);
  for(i=0; i<hsize; i++) {	  /* for each entry in table */
    if((qp=hqueue(htp, i))!=NULL) /* take queue i */
      qapply(qp, fn);		  /* apply in queue */
  }
  hunlock();
}

/* 
 * hsearch -- find an entry matching key. We don't need to include the
 *            keylen in the searchfn call because that function has
 *            knowledge of what a key actually is.
 */ 
void* shsearch(shashtable_t *htp, 
              int (*searchfn)(void *ep, const void *keyp),
	      int key) {
  int index;
  void *qp,*ep;
  
  hlock();
  index=hashfn(htp, key); /* get queue index */
  qp=hqueue(htp, index);
  ep=qsearch(qp, searchfn, (const void *)&key);
  hunlock();
  return ep;
}


/* 
 * hremove -- find an entry matching key. We don't need to include the
 *            keylen in the searchfn call because that function has
 *            knowledge of what a key actually is.
 */ 
void* shremove(shashtable_t *htp, 
              int (*searchfn)(void* ep, const void *keyp),
              int key) {
  int index;
  void *qp,*ep;
  
  hlock();
  index=hashfn(htp, key);	/* get queue index */
  qp=hqueue(htp, index);
  ep=qremove(qp, searchfn, (const void *)&key);
  hunlock();
  return ep;
}

/* END OF PUBLIC SECTION */
