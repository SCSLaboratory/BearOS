#pragma once

/* header structure: 
 * cp -- a ptr to first byte of a malloc'd chunk of bytes
 * p is a ptr to the header word for a chunk
 * size is in number of words in the chunk
 * allocated flag is the top bit of the size word = 1 for free,0 for allocated
 * a canary (the address of the slab front) is placed after the chunk
 */
#define word uint64_t                        /* heap is aligned on words */
#define HDRSIZE 1                            /* 1 word for size & sstatus */
#define CANARYSZ 1                           /* 1 word canary / slab */
#define OVERHEAD (HDRSIZE+CANARYSZ)
#define STATUSOFFSET 0                       /* offset to status word */
#define FREENEXTOFFSET 1                     /* offset to freelist ptr */
#define FREEPREVOFFSET 2                     /* previous element in freelist */
#define PREVCANARYOFFSET -1                  /* offset to previous canary */
#define SIZEMASK (0x7FFFFFFFFFFFFFFF)        /* top bit is status */
#define STATUSMASK (0x8000000000000000)     

/* byte allignment */
#define words(bytes) ((bytes+(sizeof(word)-1))/sizeof(word))
#define slabsize(words) (words+OVERHEAD)     /* chuck + overhead */

/* macros to access slab header fields */
#define headp(cp) (((word*)cp)-HDRSIZE)      /* header placed before chunk */
#define statuswordp(p) (p+STATUSOFFSET)      /* ptr to status word */
#define freenextp(p) (p+FREENEXTOFFSET)          /* ptr to free list ptr */
#define freeprevp(p) (p+FREEPREVOFFSET)      /* ptr to prev in free list */
#define chunkp(p) ((char*)(p+HDRSIZE))       /* ptr to first byte in chunk */

/* getting elements of a slab header */
#define getsize(p) (getstatusword(p) & SIZEMASK)     /* size in words */
#define getstatus(p) (getstatusword(p) & STATUSMASK) /* get top bit */
#define getstatusword(p) (*((uint64_t*)statuswordp(p)))
#define getfreenext(p) ((word*)(*freenextp(p)))
#define getfreeprev(p) ((word*)(*freeprevp(p)))
#define getnext(p) (p+slabsize(getsize(p)))    /* ptr to next slab */
#define getprev(p) ((word*)*prevcanary(p))   /* ptr to prev slab */

/* accessing the canary */
#define canaryp(p) (p+HDRSIZE+getsize(p))         /* location of canary */
#define getcanary(p) (*canaryp(p))                /* get the canary value */
#define setcanary(p) { *canaryp(p)=((word)(p)); } /* pointer to slab */

/* setting elements of a slab header */
#define setstatusword(p,w1) \
	*((uint64_t*)statuswordp(p)) = (w1)
#define setsize(p,words) {\
	setstatusword(p,(((uint64_t)words) | getstatus(p)));\
	setcanary(p);\
}
#define setfreenext(p,p1) \
	*(freenextp(p)) = ((word)p1)
#define setfreeprev(p,p1) \
	*(freeprevp(p)) = ((word)p1)

/* marking the status bit */
#define markfree(p) setstatusword(p,(getstatusword(p)|STATUSMASK)) /* bit=1 */
#define markalloc(p) setstatusword(p,(getstatusword(p)&SIZEMASK))  /* bit=0 */


/* accessing the canary of the previous slab */
#define prevcanary(p) (p+PREVCANARYOFFSET)  /* location of prev canary */

/* predicates */
#define isempty(p) (p==NULL)
#define isfree(p) (getstatus(p) > 0)
#define isalloc(p) (getstatus(p) == 0)
#define badcanary(p) (((word*)getcanary(p)) != (p))
#define notfirstslab(p) (p!=front)
#define notlastslab(p) ((p+slabsize(getsize(p)))<(front+heapsize))
