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

#include <constants.h>
#include <stdint.h>
#include <kstdio.h>
#include <asm_subroutines.h>
#include <kstring.h>
#include <kmalloc.h>
#include <kmalloc-private.h>

/*
 * kmalloc.c -- Allocates/frees a chunk of bytes aligned on word boundaries.
 * 
 * This small memory allocator is intended for use only in
 * bootstrapping.  The malloc/sbrk/vmem_alloc_region allocator will
 * eventually be replicated for use by the kernel; this will allow the
 * kernel to add pages if it runs out of memory.
 * 
 * A chunk of bytes is allocated as a slab that is prefixed by a header
 * and terminated with a canary.  All slabs lay end to end making up a
 * contiguous heap, manipulated in place, and NOT linked together. The 
 * smallest slab is two words in length.
 *
 * Each header contains the slab size, and its status (allocated or free). 
 * The canary is a pointer to the start of the slab.
 *
 * Each free slab is also connected into a doubly linked free list. The 
 * smallest slab is two words in length.
 *
 * The routines in this file are not currently re-entrant and simply use dummy 
 * routines (disable/enable) to indicate where semaphone operations need 
 * be placed.
 *
 */

/* BEGIN PRIVATE SECTION */

static word *front;		/* front of the heap */
static word *freelist;		/* free list */
static int heapsize;		/* initial heap size in words */

static int lastused;		/* debugging */
static int lastfree;

/* static functions */
static word *splitSlab(word *p, unsigned long words);
static word *heapCompact(long words);
static void freelistAdd(word *p);
static void freelistRemove(word *p);

/* 
 * ******* DEBUG *******
 * Uncomment the following line to enable KHEAPCHECKS before and after every
 * kmalloc.  Useful for debugging memory leaks and issues with context
 * switching.
 * *********************
 */
//#define KMALLOC_DEBUG 1		/* check entire heap for errors */


/* -------------TRACKING MEMORY --------------*/

#ifdef KMALLOC_TRACKING

static int mallocsites[NUMSITES];
static int freesites[NUMSITES];

void kmalloc_clear_sites(void) {
  int i;

  for(i=0; i<NUMSITES; i++) {
    mallocsites[i] = 0;
    freesites[i] = 0;
  }
}

static void print_site(int site,char *filenm) {
  if(mallocsites[site]>0 || freesites[site]>0)
    kprintf("%s:\tmallocs: %d,\tfrees: %d\n", filenm,mallocsites[site],freesites[site]);
}

static void print_totals() {
  int i, summallocs,sumfrees;
  kprintf("Site Totals:\n");
  summallocs=0, sumfrees=0;
  for(i=0; i<NUMSITES; i++) {
    summallocs += mallocsites[i];
    sumfrees += freesites[i];
  }
  kprintf("TOTALS:\t\t\tmallocs: %d,\tfrees: %d\n", summallocs,sumfrees);
}

void print_sites() {
  print_site(VPROC_SITE,            "vproc.c           ");
  print_site(HYPV_SITE,             "hypv.c            ");
  print_site(KERNEL_SITE,           "kernel.c          ");
  print_site(KVMEM_SITE,            "kvmem.c           ");
  print_site(SYS_TASK_SITE,         "sys_task.c        ");
  print_site(KWAIT_SITE,            "kwait.c           ");
  print_site(KMSG_SITE,             "kmsg.c            ");
  print_site(PROCMAN_SITE,          "procman.c         ");
  print_site(KTIMER_SITE,           "ktimer.c          ");
  print_site(DIVERSITY_SITE,        "diversity.c       ");
  print_site(REFRESH_SITE,          "refresh.c         ");
  print_site(RANDOM_SITE,           "random.c          ");
  print_site(INTERRUPTS_SITE,       "interrupts.c      ");
  print_site(ACPI_SITE,             "acpi.c            ");
  print_site(ELF_LOADER_SITE,       "elf_loader.c      ");
  print_site(FILE_ABSTRACTION_SITE, "file_abstraction.c");
  print_site(KQUEUE_SITE,           "kqueue.c          ");
  print_site(VECTOR_SITE,           "vector.c          ");
  print_site(VMEM_LAYER_SITE,       "vmem_layer.c      ");
  print_site(PCI_SITE,              "pci.c             ");
  print_site(KHASH_SITE,            "khash.c           ");
  print_site(SEMAPHORE_SITE,        "semaphore.c       ");
  print_totals();
}

void *kmalloc_track_site(int site, int bytes) {
  int wds;

  if(site<0 || site>=NUMSITES) {
    kprintf("tracking site failure\n");
    panic();
  }
  if(bytes>0) {			/* legit malloc? */
    wds = words(bytes);		/* how many words needed? */
    if (wds < 2) 
      wds = 2;			/* min chunk size */
    mallocsites[site] += wds;
  }
  return kmalloc(bytes);
}

int kfree_track_site(int site, void *cp) {
  word *p;

  if(site<0 || site>=NUMSITES) {
    kprintf("tracking site failure\n");
    panic();
  }
  if(cp!=NULL) {
    p=headp(cp);
    freesites[site] += getsize(p);
  }
  return kfree(cp);
}

#endif
/* ----------------------------------*/

/* BEGIN PUBLIC SECTION */

/*
 * kheapinit -- initializes the heap, places it on the freelist, 
 * and returns its size in words
 * Interrupts must be turned off when this is run
 */
void kheapinit(uint64_t *heap,int wds) { /* initialize the heap */
#ifdef DEBUG
	kprintf("[KMALLOC] Heap Num pages %x \n", wds*8);
#endif
  if(wds>0) {
#ifdef KMALLOC_TRACKING
    kmalloc_clear_sites();
#endif
    kmemset( heap, 0, wds*sizeof(word) );
    lastused=0;			/* debugging */
    lastfree=wds;
    heapsize=wds;                        /* save the heapsize */
    front = (word*)heap;                 /* save front of heap */
    freelist=NULL;                       /* set up the freelist */
    setsize(front,(wds-OVERHEAD));       /* entire heap is one slab */
    freelistAdd(front);                  /* put slab in freelist */
  }
}

/* Used by lwip */
void *kcalloc(int bytes) {
  uint8_t *buf;
  buf = (uint8_t*)kmalloc(bytes);
  kmemset(buf,0,bytes);
  return (void*)buf;
}


/*used by vector.c */
void *krealloc(void *mem_old, int size_new){
  uint64_t size_old;
  void *mem_new;
  void *ret;

  size_old = getsize(mem_old); 
  
  if((!mem_old) || (!size_new))
    ret=NULL;
  else if(size_old > size_new)  /*prevent stupidity */
    ret=mem_old;
  else if(size_new > size_old){ /* for now just allocate a new and copy */
    mem_new = kmalloc(size_new);
    kmemcpy( mem_new, mem_old, size_old);
    kfree(mem_old);
    ret=mem_new;
  }
  return ret;
}



/*
 * kmalloc -- allocates a chunk of bytes and returns a pointer to it.
 */
void *kmalloc(int bytes) {
  char *cp;
  unsigned long wds; 
  word *p;

#ifdef KMALLOC_DEBUG
  if (!kheapcheck(0,"kmalloc begin")) {
    kprintf("KMALLOC: Bad Heap before malloc!\n");
    panic();
  }
#endif 
  cp=NULL;			/* default is not allocated */
  if(bytes>0) {			/* legit malloc? */
    wds = words(bytes);		/* how many words needed? */
    if (wds < 2) wds = 2;	/* min chunk size */
    /* search for the first big enough free slab, f follows p */
    for(p=freelist; p!=NULL && getsize(p)<wds; p=getfreenext(p))
      ;
    if(p!=NULL) {		/* found a free chunk */
      p=splitSlab(p,wds);	/* split the existing slab */
      cp=chunkp(p);		/* find out where chunk is */
    }
    else if((p=heapCompact(wds))) { /* compact heap or give up */
      cp=chunkp(p);		    /* find out where chunk is */
    }  
  }
#ifdef KMALLOC_DEBUG
  if (!kheapcheck(0,"kmalloc end")) {
    kprintf("KMALLOC: Bad Heap after malloc!\n");
    panic();
  }
#endif 
  return cp;                               /* return pointer to the chunk */
}

/* 
 * kfree -- frees a chunk of memory by merging its slab with any free
 * slabs to its left of right in memory, then puts the combined slab on
 * the free list -- goal is to avoid fragmentation.
 */
int kfree(void *cp) {
  word *p,*pp,*np;
  int checked,newsize;
  
#ifdef KMALLOC_DEBUG
  if (!kheapcheck(0,"kfree begin")) {
    kprintf("KFREE: Bad Heap before free!\n");
    panic();
  }
#endif 
  checked=TRUE;
  if(cp!=NULL) {
    p = headp(cp);		/* find header  in chunk */
    if(badcanary(p)) {		/* check for heap overflow */
      checked=FALSE;
      kprintf("kheapcheck2 - overflow"); /* panic for now */
      panic();
    }
    newsize=getsize(p);		/* get size of slab being freed */
    if(notfirstslab(p)) {	/* try merging left, up heap */
      pp=getprev(p);		/* find previous chunk in memory */
      if(isfree(pp)) {		/* if its is already free */
	freelistRemove(pp);	/* remove it from freelist */
	newsize+=slabsize(getsize(pp)); /* get size of combined slab */
	setsize(pp,newsize);		/* update size in leftmost slab*/
	p=pp;			/* front of combined slab is p */
      }
    }
    if(notlastslab(p)) {	/* try merging right, down heap */
      np=getnext(p);		/* find the next slab */
      if(isfree(np)) {		/* if its already free */
	freelistRemove(np);	/* remove it from freelist */
	newsize+=slabsize(getsize(np)); /* update combined size */
	setsize(p,newsize);		/* update size in p */
      }	/* p remains front of new slab */
    }
    freelistAdd(p);		/* put composite in freelist */
  }
#ifdef KMALLOC_DEBUG
  if (!kheapcheck(0,"kfree end")) {
    kprintf("KFREE: Bad Heap after free!\n");
    panic();
  }
#endif 
  return checked;
}

/*
 * kheapcheck -- if printout=1, prints results of check; otherwise
 * works silently and only prints if there is an error
 */
int kheapcheck(int printout,char *msg) {
  word *p,*endp;
  int sizecheck, cntcheck;
  int sumsize, totalchunks;
  int freesize, freechunks;
  int usedsize, usedchunks;
  int freelistsize, freelistchunks;

  sumsize=0;
  totalchunks=0;
  freesize=0;
  freechunks=0;
  usedsize=0;
  usedchunks=0;
  freelistsize=0;
  freelistchunks=0;

  sizecheck=TRUE;
  cntcheck=TRUE;
  for(p=front,endp=front+heapsize; p<endp; p=getnext(p)) {
    if(badcanary(p)) {		/* uses size in header */
      kprintf("kheapcheck1 - overflow"); 
      panic();
    }
    sumsize += slabsize(getsize(p)); /* add sizes */
    totalchunks++;
    if(isfree(p)) {
      freesize += slabsize(getsize(p)); 
      freechunks++;
    }
    if(isalloc(p)) {
      usedsize += slabsize(getsize(p));
      usedchunks++;
    }
  }
  freelistsize=0;
  freelistchunks=0;
  for(p=freelist; p!=NULL; p=getfreenext(p)) {
    freelistsize += slabsize(getsize(p));
    freelistchunks++;
  }
  if((sumsize!=heapsize) || 	/* not all memory in heap */
     ((freesize+usedsize)!=heapsize) || /* something incorrectly marked */
     (freelistsize!=freesize)) { /* free list and free size differ */
    sizecheck=FALSE;
  }
  if ((printout==1) || (sizecheck==FALSE)) {
    kprintf("%s:\n[sz=%d fnd=%d a=%d f=%d flist=%d ; ainc=%d,finc=%d]\n",
	    msg,heapsize,sumsize,usedsize,freesize,freelistsize,
	    usedsize-lastused,freesize-lastfree);
    lastused=usedsize;
    lastfree=freesize;
#ifdef KMALLOC_TRACKING
    print_sites();
    kmalloc_clear_sites();
#endif
    //    kprintf("expect: sz=fnd=a+f and f=flist\n");
  }
  if(((freechunks+usedchunks)!=totalchunks) || /* incorrect number of chunks */
     (freechunks!=freelistchunks)) {	       /* not all free in freelist */
    cntcheck=FALSE;
  }
  if ((printout==1) || (cntcheck==FALSE)) {
    //    kprintf("%s: [chunks=%d a=%d f=%d,flist=%d]\n",
    //	    msg,totalchunks,usedchunks,freechunks,freelistchunks);
    //    kprintf("expect: chunks=a+f and f=flist\n");
  }
  if(!sizecheck || !cntcheck) {
    //    asm volatile("xchg %bx,%bx");
        kprintf("HEAP INVALID!\n");	/* panic for now */
	panic();
  }
  return (sizecheck && cntcheck);
}

/* static functions */

static word *splitSlab(word *p, unsigned long neededwords) {
  word *p1;
  int psize,nsize;
  
  psize=slabsize(getsize(p));
  nsize=slabsize(neededwords);
  /* worth splitting if remainder large enough for a new slab */
  if((psize-nsize)>(slabsize(2))) {        /* HACK: nsize was neededwords */
    p1=p+psize-nsize;                    /* p1 is new slab */
    setsize(p1,neededwords);             /* set slab sizes */
    setsize(p,psize-(nsize+OVERHEAD));   /* p now smaller */
    p=p1;                                /* return p1 */
  }
  else 
    freelistRemove(p);
  markalloc(p);
  return p;
}

static word *heapCompact(long words) {
  kheapcheck(1,"out of memory");
  kprintf("PANIC: Out of kernel memory\n");
  panic();
  return NULL;
}


/* 
 * Free list manipulation
 */
static void freelistAdd(word *p) { /* add p to front of freelist */
  markfree(p);			   /* status of p becomes free */
  if(freelist!=NULL)		   /* need to link into existing? */
    setfreeprev(freelist,p);	   /* old first points back to p */
  setfreenext(p,freelist);	   /* old first becomes next */
  setfreeprev(p,NULL);		   /* nothing before p in freelist */
  freelist = p;			   /* p at front of freelist */
}

static void freelistRemove(word *p) {
  word *nextp,*prevp;
  
  nextp = getfreenext(p);
  prevp = getfreeprev(p);
  if(prevp!=NULL) 
    setfreenext(prevp,nextp);
  else
    freelist=nextp;
  if(nextp!=NULL)
    setfreeprev(nextp,prevp);
}


