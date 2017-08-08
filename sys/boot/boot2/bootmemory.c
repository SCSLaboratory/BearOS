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

/* memory.c -- set up kernel memory before booting the hypervisor. */
#include <asm_subroutines.h>
#include <constants.h>
#include <kstdio.h>
#include <kstring.h>
#include <memory.h>
#include <ramio.h>
#include <pci.h>

/* A clang bug means this has to be moved here for now.
 * See: http://llvm.org/bugs/show_bug.cgi?id=9297
 */
static const char *types[] = { "",
                               "Usable",
                               "Unusable (reserved)",
                               "ACPI reclaimable",
                               "ACPI NVS",
                               "Bad" 
};

static void boot_setup_table( void *phys, union pt_entry *entry, uint64_t flags) {

  kmemset(entry, 0, sizeof(union pt_entry));
  
  entry->present = 1;
  entry->rw = flags & PG_RW ? 1 : 0;
  entry->nx = flags & PG_NX ? 1 : 0;
  entry->us = flags & PG_USER ? 1 : 0;
  ((union page*)entry)->global = flags & PG_GLOBAL ? 1 : 0;
  entry->addr = ADDR2TABLE((uint64_t)phys);
}

#define RAMDISK_VADDR 0x200000

/* this function identity maps the frames where the ramdisk is physically 
   stored so that we have a virtual mapping to it. */
static void page_ramdisk( uint64_t base, uint64_t length ) {

  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;

  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;
  
  uint64_t frame_position, old_chunklen;

  struct memmap *chunk = (struct memmap *)MEMORY_MAP;
  uint16_t *numchunks = (uint16_t *)MEMORY_MAP_ENTRIES;

  union page *page;
  int i,j;

#ifdef DEBUG
  kputs( "[Boot] Adding RAMDISK to page system" ); 
#endif

  chunk = (struct memmap *)MEMORY_MAP;
  /** first, find a chunk of physical memory suitable for storing the 
      frame array. the first chunk is not suitable bc we've used stuff 
      there already. Bios deamons and init tables.. .here be dragons **/
  chunk += 1;

  for(i = 1; i < *numchunks; i++, chunk++ ) {
    if(chunk->length == 0)
      continue;

    /* if the chunk not useable or not big enough */
    /*   it needs to be long enough just for the paging structures for the 
	 ramdisk. */
    if ( (chunk->type != 1) || (chunk->length <= 10000/*length*/ ) ) 
      continue;

    /* we found a suitable chunk */
    frame_position = chunk->base; 

    /** The point of this block of code is to verify that we've actually 
	mapped in the frames for the  virtual address which will store the 
	frame array itself. we're pretty sure that it is, but this makes it 
	portable in the future **/

    /** figure out where in the tables we will store the array based on the 
	fixed virtual address given **/
    pml4t_idx = virt2pml4t(RAMDISK_VADDR);
    pdpt_idx = virt2pdpt(RAMDISK_VADDR);
    pd_idx = virt2pd(RAMDISK_VADDR);
    pt_idx = virt2pt(RAMDISK_VADDR);

    /* page table was identity mapped at INIT_PAGE_PML4T */
    pml4t = (struct page_map_level_4_table *)INIT_PAGE_PML4T;
    if(!(pml4t->entries[pml4t_idx]).present){
      boot_setup_table( (void *)frame_position, (union pt_entry *)PML4TE2vaddr(pml4t_idx), PG_RW);
      /**then memset the frame for the pdpt ITSELF to zero */
      kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;
    }
    pdpt = (struct page_directory_pointer_table *)PDPTE2vaddr(pml4t_idx,0);
    if(!(pdpt->entries[pdpt_idx]).present){
      boot_setup_table( (void *)frame_position, (union pt_entry *)PDPTE2vaddr(pml4t_idx,pdpt_idx), PG_RW); 
      kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;
    }
    pd = (struct page_directory *)PDE2vaddr(pml4t_idx,pdpt_idx,0);
    if(!(pd->entries[pd_idx]).present){
      boot_setup_table((void *)frame_position,  (union pt_entry *)PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), PG_RW);
      kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;
    }
    /* make the length page aligned */
    if(length % PAGE_SIZE) 
      length += PAGE_SIZE - ( length%PAGE_SIZE ) ;

    /** This loops maps in the needed number of pages for the region, which is 
	done by accessing the frame_array. **/

    for ( j = 0; j < length/PAGE_SIZE; j++ ) {

      if(pt_idx == 512) {
        pt_idx = 0;
        pd_idx++;

        if(pd_idx == 512) {
          pd_idx = 0;
          pdpt_idx++;

          if(pdpt_idx == 512) {
            pdpt_idx = 0;
            pml4t_idx++;

            if(pml4t_idx == 511) {
              kputs("Not enough virtual memory to page the region");
              panic();
            }

            /**add a new pdpt to the pml4t **/ 
            boot_setup_table( (void *)frame_position, (union pt_entry *)PML4TE2vaddr(pml4t_idx), PG_RW); 
            /**then memset the frame for the pdpt ITSELF to zero */
            kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
            frame_position += PAGE_SIZE;
          }//pdpt's

          /** add a new pdt to the pdpt */      
          boot_setup_table( (void *)frame_position, (union pt_entry *)PDPTE2vaddr(pml4t_idx,pdpt_idx), PG_RW); 
          kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
          frame_position += PAGE_SIZE;
        }//pd's

        /** add a new pt to the pdt */
        boot_setup_table((void *)frame_position,  (union pt_entry *)PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), PG_RW); 
        kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);
        frame_position += PAGE_SIZE;
      }//pt's 

      /** finally attach the frames to actual pt's */
      page = (union page *)PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx, pt_idx); 

      page->present = 1;
      page->rw = 1; /* 1 for read-write, 0 for read-only. */
      page->nx = 0; /* executable because these are for the kernel */
      page->global = 1;
      page->addr = ADDR2TABLE(base + (j*PAGE_SIZE));
      pt_idx++;
    }//for loop on framearray_pages
    break;
  }/*close the chunk search for loop */

  /**reset the chunk to the beginning of the array **/
  chunk = (struct memmap *)MEMORY_MAP;

  /** Now that we've put some stuff in this area, we don't want the 
      kernel to stomp on it. But the chunk was marked as useable so
      the kernel might stomp on it... We need to seperate the current 
      chunk into 2 chunks: useable and reclaimable. **/

  /* move the rest of the chunks to make room for our new chunk in the array */
  for(j = *numchunks +1; j > i; j--){
    chunk[j] = chunk[j-1];
  }

  /* overwrite the current chunk entry with new properties */
  /* base stays the same */
  old_chunklen =  chunk[i].length;
  chunk[i].length = 10 * PAGE_SIZE;
  chunk[i].type = 6; /* TODO: Remove magic numbers */

  /* write the new chunk information as the remainder of current chunk */
  chunk[i+1].base = chunk[i].base + chunk[i].length; 
  chunk[i+1].length = old_chunklen - chunk[i].length;
  chunk[i+1].type = 1; 

  /* increment the number of chunks in the memmap */
  *numchunks = *numchunks + 1;

  return;
}

/** this function finds the ramdisk, pages the ramdisk, them initialized the 
    frame array. */
void bootmemory_init( void ) {
  struct memmap *chunks = (struct memmap *)MEMORY_MAP;
  uint16_t *numchunks = (uint16_t *)MEMORY_MAP_ENTRIES;
  int i;

  uint64_t ramdisk_chunk_base = 0;
  uint64_t ramdisk_chunk_length = 0;
  uint32_t ramdisk_chunk_type = 0;
	
#ifdef DEBUG
  kputs("[Boot] Scanning memory...");
#endif

  for(i = 0; i < *numchunks; i++, chunks++) {

    if(chunks->length == 0)
      continue;

#ifdef DEBUG
    kprintf("[Boot] Memory: Start (%x); Length (%x)\n"
	    "    %s\n", chunks->base, chunks->length, types[chunks->type]);
#endif

    if( (chunks->length == 0xA00000/*size of memdisk*/) && (chunks->type == 2) ) {

      ramdisk_chunk_base = chunks->base;
      ramdisk_chunk_type = chunks->type;
      ramdisk_chunk_length = chunks->length; 
#ifndef DEBUG
      break;
#endif
    }
  }

#ifdef DEBUG
  kprintf("[Boot] Memory scan complete.\n");

  kprintf( "[Boot] ramdisk Start (%x); Length (%x) - %s\n", 
	   ramdisk_chunk_base,
	   ramdisk_chunk_length,
	   types[ramdisk_chunk_type] );
#endif

  /* Set ramdisk variables. */
  *((uint64_t *)RAMDISK) = ramdisk_chunk_base;

  /** IMPORTANT: Don't change the order of page_ramdisk(..) and 
      frame_array_init(). page ramdisk does not use the frame array to do 
      its allocation, so in order for the frame array to account for the 
      ramdisk, let the ramdisk do its thing first, then the frame array will 
      account for it **/

  /* page in the ramdisk */
  page_ramdisk( ramdisk_chunk_base, ramdisk_chunk_length );

  /** mark the first entry in the memmap as "acpi reclaimable"... we don't 
      want to leave it as strictly useable because there is a bunch of stuff 
      there that should not be overwritten by the kernel. */ 
  ((struct memmap *)MEMORY_MAP)->type = 0x6;

  return;  
}
