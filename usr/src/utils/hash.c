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
 * hash.c -- implements a generic hash table as an indexed set of queues.
 *
 */
#include <stdlib.h>
#include <stdint.h>
#include <utils/queue.h>
#include <utils/hash.h>

#define hlock()
#define hunlock()


/* PRIVATE SECTION */

/* the hidden structre of a hash table */
typedef struct {
  uint32_t table_size;		/* the size of the table */
  void **table;			/* pointer to a table of void* */
} hhash_t;

/* accessor macros */
#define hsize(htp) (((hhash_t*)htp)->table_size)
#define htable(htp) (((hhash_t*)htp)->table)
#define hqueue(htp,qindex) (*(htable(htp)+qindex))

/* The following (rather complicated) code, between the dashed line
 * marks, has been taken from Paul Hsieh's website. It is under the
 * terms of the BSD license. It's a really good hash function used all
 * over the place nowadays, including Google Sparse Hash. 
*/
/*----------------------------------------------------------------*/
#define get16bits(d) (*((const uint16_t *) (d)))

static uint32_t SuperFastHash (const char *data, int len) {
  uint32_t hash = len, tmp;
  int rem;
  
  if (len <= 0 || data == NULL) return 0;
  rem = len & 3;
  len >>= 2;
  /* Main loop */
  for (;len > 0; len--) {
    hash  += get16bits (data);
    tmp    = (get16bits (data+2) << 11) ^ hash;
    hash   = (hash << 16) ^ tmp;
    data  += 2*sizeof (uint16_t);
    hash  += hash >> 11;
  }
  /* Handle end cases */
  switch (rem) {
  case 3: hash += get16bits (data);
    hash ^= hash << 16;
    hash ^= data[sizeof (uint16_t)] << 18;
    hash += hash >> 11;
    break;
  case 2: hash += get16bits (data);
    hash ^= hash << 11;
    hash += hash >> 17;
    break;
  case 1: hash += *data;
    hash ^= hash << 10;
    hash += hash >> 1;
  }
  /* Force "avalanching" of final 127 bits */
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;
  return hash;
}
/*-----------------------------------------------------------------*/

static uint32_t hashfn(hashtable_t *ctx, const char *key, int keylen) {
  uint32_t hash = SuperFastHash(key, keylen);
  return (hash % hsize(ctx));
}

/* END OF PRIVATE SECTION */



/* PUBLIC SECTION */

hashtable_t *hopen(uint32_t hsize) {
  hhash_t *htp;
  void **tp, **p, **endp;
  
  hlock();
  htp = malloc(sizeof(hhash_t));	  /* the hash table */
  tp = malloc(sizeof(void*)*hsize);       /* indexed set of queues */
  hsize(htp) = hsize;
  htable(htp) = tp;
  for(p=tp, endp=tp+hsize; p<endp; p++)
    *p=qopen();			/* each entry is a queue */
  hunlock();
  return (hashtable_t*)htp;
}

void hclose(hashtable_t *htp) {
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
void hput(hashtable_t *htp, void *ep, const char *key, int keylen) {
  uint32_t index;
  void *qp;
  
  hlock();
  index=hashfn(htp, key, keylen);	   /* get queue index */
  qp=hqueue(htp, index);                   /* find queue */
  qput(qp, ep);                            /* put in queue */
  hunlock();
}

/*
 * happly -- apply a function to every entry in the table
 */
void happly(hashtable_t *htp, void (*fn)(void *ep)) {
  uint32_t i,hsize;
  void *qp;
  
  hlock();
  hsize = hsize(htp);
  for(i=0; i<hsize; i++) {	/* for each entry in table */
    qp=hqueue(htp, i);		/* take queue i */
    if(qp != NULL)		/* (may be empty) */
      qapply(qp, fn);		/* apply in queue */
  }
  hunlock();
}

/* 
 * hsearch -- find an entry matching key. We don't need to include the
 *            keylen in the searchfn call because that function has
 *            knowledge of what a key actually is.
 */ 
void* hsearch(hashtable_t *htp, 
              int (*searchfn)(void *elementp, const void *searchkeyp),
              const char *key, int keylen) {
  uint32_t index;
  void *qp,*ep;
  
  hlock();
  index=hashfn(htp, key, keylen); /* get queue index */
  qp=hqueue(htp, index);
  ep=qsearch(qp, searchfn, key);
  hunlock();
  return ep;
}


/* 
 * hremove -- find an entry matching key. We don't need to include the
 *            keylen in the searchfn call because that function has
 *            knowledge of what a key actually is.
 */ 
void* hremove(hashtable_t *htp, 
              int (*searchfn)(void* elementp, const void* searchkeyp),
              const char *key, int keylen) {
  uint32_t index;
  void *qp,*ep;
  
  hlock();
  index=hashfn(htp, key, keylen); /* get queue index */
  qp=hqueue(htp, index);
  ep=qremove(qp, searchfn, key);
  hunlock();
  return ep;
}

/* END OF PUBLIC SECTION */
