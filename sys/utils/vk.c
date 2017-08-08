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

#include <kstdio.h>
#include <stdint.h>
#include <kstring.h>
#include <constants.h>
#include <vk.h>
#include <vkprivate.h>
#include <asm_subroutines.h>

#define UINT32_MAX (~(uint32_t)0)

/* PUBLIC FUNCTIONS */
void vkinit(vkheap_t *hp,uint32_t heapsize,uint32_t pagesize) {
  uint8_t *map;
  int i,mapsize;

  /* sanity check -- heapsize & pagesize stored in uint32_t */
  if(heapsize < 1 || heapsize>UINT32_MAX || (pagesize < 2*sizeof(uint32_t)+1) 
     ||pagesize>UINT32_MAX)
  {
    kputs("heap pointer not aligned on a page boundary");
    panic();
  }
  if(((uint64_t)hp)%pagesize!=0) {
    kputs("heap pointer not aligned on a page boundary");
    panic();
  }
  vksize(hp)=heapsize;
  vkpagesize(hp)=pagesize;
  mapsize=vkmapsize(hp);
  /* sanity check: bits we need > space in one page ? */
  if(mapsize > (pagesize-2*sizeof(uint32_t))) {
    kputs("heap pointer not aligned on a page boundary");
    panic();
  }
  /* run over the map and initialize it to FREE - 2bits/page in the map */
  for(map=vkmap(hp), i=0; i<mapsize; i++,map++)
    *map=FREE;
  
  return;
}

/* vkmalloc -- returns a pointer to 'size' contiguous pages on a vkheap. 
 * Note: Size is typically 1.
 */
uint64_t vkmalloc(vkheap_t* hp,uint32_t size) {
  int i,j,found;
  vkpage_t* page;

  page=NULL; /* by default return null */
  found=FALSE;
  /* look in page table till found or at end of table */
  for(i=0; ((!found) && ((i+size) <= (vksize(hp)))); i++) 
    if(allfree(hp,i,size))  /* size pages beginning at i free? */
      found=TRUE;           /* i-1 is the start */
  if(found && i>=1) {
    i--;	        /* adjust for the loop above */
    for(j=i;j<(i+size);j++) /* mark page table entries allocated */
      allocpage(hp,j);
    page=vkpage(hp,i); /* return a pointer to actual page i */
  }
  return (uint64_t)page;
}

/* vkdirty -- makes the 'size' pages beginning at 'p' dirty. */
void vkdirty(vkheap_t* hp, vkpage_t* p,uint32_t size) {
  int i,index;
  index = pageindex(hp,p);
  for(i=0; i<size; i++)
    dirtypage(hp,(index+i));
  
  return;
}

/* vkfree -- makes the 'size' pages beginning at 'p' free. */
void vkfree(vkheap_t* hp, vkpage_t* p,uint32_t size) {
  int i,index;
  index = pageindex(hp,p);
  for(i=0; i<size; i++)
    freepage(hp,(index+i));
  
  return;
}

/* vkfreedirty -- makes all dirty pages in a vkheap free for reuse */
void vkfreedirty(vkheap_t* hp) {
  int i;
  for(i=0; i<vksize(hp); i++)
    if(isdirty(hp,i)) 
      freepage(hp,i);

  return;
}

/* vkshow -- shows the page map */
void vkshowmap(vkheap_t *hp) {
  uint32_t i;
  int perline;

  if(vksize(hp)<64)
    perline=4;
  else
    perline=32;
	
  for(i=0; i<vksize(hp); i++) {
    if(i%perline==0)
      kprintf("%d: ",i);
    showstate(hp,i);
    if(i%4==3)
      kprintf(" ");
    if(i%perline==perline-1 && perline!=i)
      kprintf("\n");
  }

  kprintf("\n");
  return;
}


/* PRIVATE FUNTIONS */

static int allfree(vkheap_t* hp,int start,uint32_t size) {
  int i,isallfree,last;

  last=start+size;
  isallfree = TRUE;
  /* keep going while free */
  for(i=start; ((isallfree) && (i<last)); i++) {
    if( !(isfree(hp,i)) )
      isallfree=FALSE;
  }
  return isallfree;
}

static void showstate(vkheap_t *hp,uint32_t page) {
 
  if(isfree(hp,page))
    kprintf("F");
  else	if(isdirty(hp,page))
    kprintf("D");
  else 	if(isalloc(hp,page))
    kprintf("A");
  else
    kprintf("?\n");
  
  return;
}

#if 0
/* 
 * The following functions are not used but helpful for debugging 
 */

static void byte_to_binary(uint8_t x,char str[]) {
  str[0] = '\0';
  int z;
  for (z = 128; z > 0; z >>= 1) {
    kstrncat(str, ((x & z) == z) ? "1" : "0", 9);
  }
}

static void showpages(vkheap_t *hp) {
  uint32_t size = vksize(hp);	/* allocate the largest number of pages possible */
  uint32_t pagesize = vkpagesize(hp);
  uint32_t mapsize = vkmapsize(hp);
  uint8_t *map;
  int i;
  char str[9];
	
  kprintf("Heap: size=%d, pagesize=%d, mapsize=%d\n",
	  (int)vksize(hp),(int)vkpagesize(hp),(int)vkmapsize(hp));
  /* never overrun the map */
  for(map=vkmap(hp), i=0; i<mapsize; i++, map++) {
    byte_to_binary(*map,str);
    kprintf("%6d: %s\n",i*BITSPERPAGE,str);
  }
}

#endif 
