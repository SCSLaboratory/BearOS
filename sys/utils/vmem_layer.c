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
 * vmem_layer.c
 *
 * Functions for initializing and managing the system's virtual memory. 
 *
 */

#include <asm_subroutines.h>
#include <constants.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <memory.h>
#include <khash.h>
#include <kqueue.h>
#include <vk.h>

/* 
 * we need to have these static variables because boot2 does not have a .bss
 * Ergo, we need to have setters and getters for the heap 
 */

/* Location of the heap in virtual memory. */
static uint64_t heap_start __attribute__ ((section (".bss")));
/* Is the heap ready to be used? Needed for our queues. */
static int heap_ready __attribute__ ((section (".bss")));

static uint64_t frame_array_len __attribute__ ((section (".bss")));
static uint64_t frame_array_vaddr __attribute__ ((section (".bss")));
#ifdef BOOTLOADER
static struct frame_array_entry_t *framearray __attribute__ ((section (".bss")));
static uint64_t save_frame_array_idx __attribute__ ((section (".bss")));
#else 
uint64_t save_frame_array_idx;
#endif

/* find a free frame in the framearray */
uint64_t get_free_frame(void);

/* implementation of vmem_free */
void vmem_free_imp(uint64_t* base, uint64_t length, int free);

/* Internal structure for the allocated queue, for keeping track of
 * allocated chunks. */
struct allocated_chunk {
  uint64_t base;                           /* Virtual addr */
  uint64_t size;                           /* In pages */
  uint32_t pid;                            /* Who owns this memory */
};

static void print_name(){
#ifdef KERNEL 
  kprintf("\t[Kernel vmem] ");
#endif
#ifdef HYPV 
  kprintf("\t[Hypv vmem] ");
#endif
}


/*function for flushing a page from the processor cache */
inline void cache_flush(volatile void *addr)
{
    asm volatile ("clflush (%0)" :: "r"(addr));
}

/*
 * -------------TRACKING VIRTUAL MEMORY -------------
 * See tracking macros in sys/include/kvmem_sites.h
 */
#ifdef KVMEM_TRACKING

static int allocsites[NUMVMEMSITES];
static int freesites[NUMVMEMSITES];

void vmem_clear_sites(void) {
  int i;

  for(i=0; i<NUMVMEMSITES; i++) {
    allocsites[i] = 0;
    freesites[i] = 0;
  }
}

static void print_site(int site,char *filenm) {
  if(allocsites[site]>0 || freesites[site]>0)
    kprintf("%s:\tallocs: %d,\tfrees: %d\n", filenm,allocsites[site],freesites[site]);
}

static void print_totals() {
  int i, sumallocs,sumfrees;
  sumallocs=0, sumfrees=0;
  for(i=0; i<NUMVMEMSITES; i++) {
    sumallocs += allocsites[i];
    sumfrees += freesites[i];
  }
  kprintf("TOTALS:\t\tAllocs: %d,\tFrees: %d, Taken: %d\n\n", 
	  sumallocs,sumfrees,(sumallocs-sumfrees));
}

static void print_sites(void) {
  kprintf("Site Totals:\n");
  print_site(DIVERSITY_VMEM_SITE,   "diversity.c  ");
  print_site(MEMORY_VMEM_SITE,      "memory.c     ");
  print_site(VMEM_LAYER_VMEM_SITE,  "vmem_layer.c ");
  print_site(KVMEM_VMEM_SITE,       "kvmem.c      ");
  print_site(SYS_TASK_VMEM_SITE,    "sys_task.c   ");
  print_site(PROCMAN_VMEM_SITE,     "procman.c    ");
  print_site(VPROC_VMEM_SITE,       "vproc.c      ");
  print_totals();
  vmem_print_pages(2);		/* inspect page maps */
}

/* The exported checking routine -- this function is called
 * from a designated location, prints the statistics since the 
 * last call, and resets the statistics
 */
void kvmemcheck(char *loc) {
  kprintf("%s:\n",loc);
  print_sites();		/* print the values since last check */
  vmem_clear_sites();		/* clear the values */
}
#endif
/* -------------END TRACKING CODE---------------------*/

/* Prints out the number of pages allocated and free accodring to the current 
 * state of the memory map bitmap.  The number argument just prints what 
 * direction is happening when you use it for debugging. You have to know
 * which is being called.  
 */
void vmem_print_pages(int alloc) {
  int sum_taken = 0;
  int sum_free = 0;
  
  uint64_t framearray_idx;

  //print_name();
  //kprintf("%sllocating: ", alloc ? "A" : "Dea");
  
  for ( framearray_idx = 0; framearray_idx < frame_array_len; framearray_idx++){
    if ( framearray[framearray_idx].type == 0x1 ) /* useable */ {
      framearray[framearray_idx].free ? sum_free++ : sum_taken++ ;
    }
  }

  kprintf("Page Maps: Taken %d, Free %d Total %d\n", sum_taken, sum_free, (sum_free+sum_taken));
}

void setup_table( void *phys, void *vaddr, uint64_t flags) {

  union pt_entry *entry = (union pt_entry*)vaddr;

  kmemset(entry, 0, sizeof(union pt_entry));
  
  entry->present = 1;
  entry->rw = (flags & PG_RW) ? 1 : 0;
  entry->nx = (flags & PG_NX) ? 1 : 0;
  entry->us = (flags & PG_USER) ? 1 : 0;
  ((union page*)entry)->global = ( flags & PG_GLOBAL ) ? 1 : 0;
  entry->addr = ADDR2TABLE((uint64_t)phys);
  return;
}

uint64_t get_flags( union page *p ) {

  uint64_t ret_val = 0x0;

  if ( p->nx )
    ret_val |= PG_NX;
  if ( p->rw )
    ret_val |= PG_RW;
  if ( p->us )
    ret_val |= PG_USER;
  if ( p->global )
    ret_val |= PG_GLOBAL;
  if ( p->pcd )
    ret_val |= PG_PCD;
  if ( p->pwt )
    ret_val |= PG_PWT;

  return ret_val;
}

/** probe the free frame array; return the physical address of a free frame */
uint64_t get_free_frame(void) {

  struct frame_array_entry_t* framearray = (struct frame_array_entry_t*)frame_array_vaddr;

  uint64_t i;

  for ( i = 0; i < frame_array_len; i++, save_frame_array_idx++ ) {
    if ( save_frame_array_idx >= frame_array_len )
      save_frame_array_idx = 0;

    if ( framearray[save_frame_array_idx].type != 0x1 )
      continue;

    if ( framearray[save_frame_array_idx].free ) {
      framearray[save_frame_array_idx].free = 0x0;
      return save_frame_array_idx*PAGE_SIZE;
    }
  }

  kprintf("PANIC: NO AVAILABLE FREE FRAMES\n");
  panic();

  /* never reached */
  return 0x0;
}

uint64_t get_contiguous_frames(uint64_t length) {

  struct frame_array_entry_t* framearray = (struct frame_array_entry_t*)frame_array_vaddr;
  uint64_t i, j;
  
  /*loop over the entire frame array */
  for ( i = 0; i < frame_array_len; i++ ) {
    /*if the current index  plus the length requested is greater than the 
     * overall length of the frame array we don't have enough memory */
    if ( save_frame_array_idx + (length/PAGE_SIZE) > frame_array_len )
      save_frame_array_idx = 0;
 
    for ( j = 0; j < length/PAGE_SIZE; j++, save_frame_array_idx++) {
      if (  framearray[save_frame_array_idx].type != 0x1 || 
	    !framearray[save_frame_array_idx].free ) {
        save_frame_array_idx += 1;
        break;
      }
    }

    /* If we found enough frames then J will equal the requested length
     * otherwise keep checking 
     */
    if ( j != length / PAGE_SIZE )
      continue;

    /* at the end of the inner loop, save frame array idx is one beyond the 
       end of the contiguous frame region we are going to return. 
    */

    for ( i = 1; i <= length/PAGE_SIZE; i++ )
      framearray[save_frame_array_idx - i].free = 0x0;
    
    return (save_frame_array_idx - (length/PAGE_SIZE))*PAGE_SIZE;
  }

  kputs("Get contiguous frames ran out of memory");
  panic();

  /* never reached */
  return 0x0;
}

void frame_array_init(void) {

  struct memmap *chunk = (struct memmap *)MEMORY_MAP;
  struct memmap *oldchunk;
  uint16_t *numchunks = (uint16_t *)MEMORY_MAP_ENTRIES;
  uint64_t memory_present, frame_position; 
  int framearray_pages; 
  int i, j;

  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;
  union page *page;

  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;
  uint64_t framearray_idx;
  
  int pts, pds, pdpts;

  int hole_length;

  /** This calculates the total number of bytes available in ram on the 
      system by taking the last chunks base address and adding its 
      length to it. Chunks are blocks of memory on the sytem given to us 
      by the bios from boot1.s **/
  memory_present = chunk[(*numchunks)-1].base + 
    chunk[(*numchunks)-1].length;

  /** set the number of entries that will be in the array */
  frame_array_len = memory_present / PAGE_SIZE;

  /** find the number of pages needed to store the array */
  framearray_pages = ((memory_present/PAGE_SIZE) * 
		      sizeof(struct frame_array_entry_t)) / PAGE_SIZE;
  if((memory_present/PAGE_SIZE) % framearray_pages)
    framearray_pages += 1;

  /** add the number of frames that will be needed for paging structs 
      to address the frame array */
  pts = framearray_pages % 512 ? 
    (framearray_pages / 512) + 1 : framearray_pages / 512; 
  pts += 1; /* in case it crosses a pt bountry virtually */
  /* the plus 1 is potentially wasteful... but who cares? */

  pds = pts % 512 ? (pts / 512) + 1 : pts / 512;
  pds += 1; /* in case it crosses a pd boundry virtually */
  /* the plus 1 is potentially wasteful... but who cares? */

  pdpts = pds % 512 ? (pds / 512) + 1 : pds / 512;
  pdpts += 1; /* in case it crosses a pdpt boundry virtually */
  /* the plus 1 is potentially wasteful... but who cares? */

  framearray_pages += pts + pds + pdpts; 

  /* TODO: fix the memmap when we use stuff in the first chunk to get rid 
     of this. */
  /** first, find a chunk of physical memory suitable for storing the 
      frame array. the first chunk is not suitable bc we've used stuff 
      there already. **/
  chunk += 1;

  for(i = 1; i < *numchunks; i++, chunk++ ) {

    if(chunk->length == 0)
      continue;

    /* if the chunk is useable & big enough*/
    if ( (chunk->type != 1) || 
	 (chunk->length <= (framearray_pages * PAGE_SIZE)) ) 
      continue;

    frame_position = chunk->base;
      
    /** figure out where in the tables we will store the array based on the 
        fixed virtual address given **/
    pml4t_idx = virt2pml4t(frame_array_vaddr);
    pdpt_idx = virt2pdpt(frame_array_vaddr);
    pd_idx = virt2pd(frame_array_vaddr);
    pt_idx = virt2pt(frame_array_vaddr);

    /** NOTE: For setup tables in this function, we are marking them as 
	executable because this table might be used for some executable stuff 
	later on (we dont know) and we dont trust later allocators to check 
	this top level permissions. marking the page as no-execute is
	sufficient to ensure that the frame array is not executable **/
    
    /* initialize the virtual adderss */
    pml4t = (struct page_map_level_4_table *)read_cr3();
    if(!(pml4t->entries[pml4t_idx]).present){
      setup_table( (void *)frame_position, PML4TE2vaddr(pml4t_idx), 1/* executable */);
      /**then memset the frame for the pdpt ITSELF to zero */
      kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;  
    }

    pdpt = (struct page_directory_pointer_table *)PDPTE2vaddr(pml4t_idx,0);
    if(!(pdpt->entries[pdpt_idx]).present){
      setup_table( (void *)frame_position, PDPTE2vaddr(pml4t_idx,pdpt_idx), 1 /* executable */); 
      kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;
    }

    pd = (struct page_directory *)PDE2vaddr(pml4t_idx,pdpt_idx,0);
    if(!(pd->entries[pd_idx]).present){
      setup_table((void *)frame_position, PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), 1 /* executable */);
      kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);
      frame_position += PAGE_SIZE;        
    }

    /** this loop maps in the frames to store enough pages to contain the 
	frame array **/
    for ( i = 0; i < framearray_pages; i++ ) {
        
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
              kputs("Not enough virtual memory to cover the largest chunk!");
              panic();
            }

            /**add a new pdpt to the pml4t **/ 
            setup_table( (void *)frame_position, PML4TE2vaddr(pml4t_idx), PG_RW); 
            /**then memset the frame for the pdpt ITSELF to zero */
            kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
            frame_position += PAGE_SIZE;
          }//pdpt's

          /** add a new pdt to the pdpt */      
          setup_table( (void *)frame_position, PDPTE2vaddr(pml4t_idx,pdpt_idx), PG_RW); 
          kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
          frame_position += PAGE_SIZE;
        }//pd's

        /** add a new pt to the pdt */
        setup_table((void *)frame_position, PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), PG_RW); 
        kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);
        frame_position += PAGE_SIZE;
      }//pt's 

      /** finally attach the frames to actual pt's */
      setup_table((void*)frame_position, PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx, pt_idx), PG_RW | PG_NX | PG_GLOBAL );
      frame_position += PAGE_SIZE;
      pt_idx++;
    }//for loop on framearray_pages     

    /* we found our good chunk and finished initialization: break! */
    break;  

  } // chunks for loop



  /** now actually populate the array **/
  framearray = (struct frame_array_entry_t*)frame_array_vaddr;

  framearray_idx = 0;
  chunk = (struct memmap *)MEMORY_MAP;
  for( i = 0; i < *numchunks; i++ ) {

    for ( j = 0 ; j < chunk->length/PAGE_SIZE; j++ ) {
      if ( chunk->type == 0x1 ) 
        framearray[framearray_idx].free = 0x1;
      else 
        framearray[framearray_idx].free = 0x0;

      framearray[framearray_idx++].type = chunk->type;
    }

    /** increment to next chunk */
    oldchunk = chunk++;
    
    /** check to see if there is a hole */
    if ( (oldchunk->length + oldchunk->base) == chunk->base )
      continue;
    if ( i == (*numchunks - 1) ) /** if this is the last chunk */
      break; 
    
    /** there is a hole... */
    hole_length = (chunk->base-(oldchunk->length + oldchunk->base))/PAGE_SIZE;
    for ( j = 0; j < hole_length; j++ ) {

      framearray[framearray_idx].free = 0x0; /* no hole is free (TWSS) */
      framearray[framearray_idx++].type = 0x06; /* TODO: Fix the chunk types enum situation */
    }
  } /* end the loop iterating over chunks to populate framearray */

  /** Now we want to actually fill the frame array entries with the vaddr that 
      is mapped to the frame, so we'll walk the page tables to find currently 
      present mappings **/
  pml4t = (struct page_map_level_4_table*)INIT_PAGE_PML4T;
  for ( pml4t_idx = 0; pml4t_idx < 512; pml4t_idx++ ) {

    if ( !pml4t->entries[pml4t_idx].present )
      continue;

    for ( pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++ ) {

      pdpt = (struct page_directory_pointer_table*)PDPTE2vaddr(pml4t_idx,0);
      if ( !pdpt->entries[pdpt_idx].present )
        continue;

      for ( pd_idx = 0; pd_idx < 512; pd_idx++ ) {

        pd = (struct page_directory*)PDE2vaddr(pml4t_idx,pdpt_idx,0);
        if ( !pd->entries[pd_idx].present )
          continue;

        for ( pt_idx = 0; pt_idx < 512; pt_idx++ ) {

          pt = (struct page_table*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0);
          if ( pt->entries[pt_idx].present ) {

            page = (union page *)PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx, pt_idx);

            /* store virtual address and mark as taken */
            framearray[TABLE2ADDR(page->addr)/PAGE_SIZE].vaddr = 
              idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx);
            framearray[TABLE2ADDR(page->addr)/PAGE_SIZE].free = 0x0;
          }

        } /* end pt level loop */
      } /* end pd level loop */
    } /* end pdpt level loop */ 
  } /* end pml4t level loop */

  return;
}

/** given a virtual address, make sure that there are paging structures mapped 
    to support writing to that address. */
void vaddr_init(uint64_t *vaddr, uint64_t flags) {

  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;

  uint64_t frame_paddr;

  /** figure out where in the tables we will store the array based on the 
      fixed virtual address given **/
  int pml4t_idx = virt2pml4t(vaddr);
  int pdpt_idx = virt2pdpt(vaddr);
  int pd_idx = virt2pd(vaddr);

  /** NOTE: For setup tables in this function, we are marking them as 
      executable because this table might be used for some executable stuff 
      later on (we dont know) and we dont trust later allocators to check this 
      top level permissions. marking the pte only as no-execute is sufficient
      to ensure that given memory is not executable **/
  pml4t = (struct page_map_level_4_table *)phys2virt(read_cr3());
  if(!(pml4t->entries[pml4t_idx]).present){
    frame_paddr = get_free_frame();
    setup_table( (void*)frame_paddr, PML4TE2vaddr(pml4t_idx), PG_RW | PG_USER);
    framearray[frame_paddr/PAGE_SIZE].vaddr = PDPTE2vaddr(pml4t_idx,0);
    /**then memset the frame for the pdpt ITSELF to zero */
    kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
  }

  /* probe the pdpt to verify that we have a pd */
  pdpt = (struct page_directory_pointer_table *)PDPTE2vaddr(pml4t_idx,0);
  if(!(pdpt->entries[pdpt_idx]).present){
    frame_paddr = get_free_frame();
    setup_table( (void*)frame_paddr, PDPTE2vaddr(pml4t_idx,pdpt_idx), PG_RW | PG_USER); 
    framearray[frame_paddr/PAGE_SIZE].vaddr = PDE2vaddr(pml4t_idx,pdpt_idx,0);
    kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
  }

  /* probe the pd to verify that we have a pt */
  pd = (struct page_directory *)PDE2vaddr(pml4t_idx,pdpt_idx,0);
  if(!(pd->entries[pd_idx]).present){
    frame_paddr = get_free_frame();
    setup_table((void *)frame_paddr, PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), PG_RW | PG_USER);
    framearray[frame_paddr/PAGE_SIZE].vaddr = PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0);
    kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);
  }

  return;
}

void vmem_alloc( uint64_t* base, uint64_t length, uint64_t flags ) {

  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;
	
  uint64_t frame_paddr;

  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;

  vaddr_init(base, flags);

  /** figure out where in the tables we will store the array based on the 
      fixed virtual address given **/
  pml4t_idx = virt2pml4t(base);
  pdpt_idx = virt2pdpt(base);
  pd_idx = virt2pd(base);
  pt_idx = virt2pt(base);

  while ( (uint64_t)idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx) < ((uint64_t)base+length) ) {

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
	    kprintf("args to vmem alloc were: \n");
	    kprintf("     base: 0x%x\n", base);
	    kprintf("     length: 0x%x\n", length);
	    kprintf("     flags: 0x%x\n", flags);
            panic();
          }

	  /* if we rolled over the indeces above, the condition tested in the 
	     while loop wasn't an accurate test of what we're trying to do */
	  if ( !((uint64_t)idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx) < ((uint64_t)base+length)) )
	    break;

          /**add a new pdpt to the pml4t **/ 
	  pml4t = (struct page_map_level_4_table *)phys2virt(read_cr3());
	  if(!(pml4t->entries[pml4t_idx]).present) {
	    frame_paddr = get_free_frame();
	    setup_table( (void *)frame_paddr, PML4TE2vaddr(pml4t_idx), PG_RW | PG_USER); 
	    framearray[frame_paddr/PAGE_SIZE].vaddr = PDPTE2vaddr(pml4t_idx,0);
      	    /**then memset the frame for the pdpt ITSELF to zero */
	    kmemset(PDPTE2vaddr(pml4t_idx,0), 0, PAGE_SIZE);
	  }
        }//pdpt's

        /** add a new pdt to the pdpt */    
	pdpt = (struct page_directory_pointer_table *)PDPTE2vaddr(pml4t_idx,0);
	if(!(pdpt->entries[pdpt_idx]).present) {
	  frame_paddr = get_free_frame();  
	  setup_table( (void *)frame_paddr, PDPTE2vaddr(pml4t_idx,pdpt_idx), PG_RW | PG_USER); 
	  framearray[frame_paddr/PAGE_SIZE].vaddr = PDE2vaddr(pml4t_idx,pdpt_idx,0);
	  kmemset(PDE2vaddr(pml4t_idx,pdpt_idx,0), 0, PAGE_SIZE);
	}
      }//pd's

      /** add a new pt to the pdt */
      pd = (struct page_directory *)PDE2vaddr(pml4t_idx,pdpt_idx,0);
      if(!(pd->entries[pd_idx]).present) {

	frame_paddr = get_free_frame();	
	setup_table((void *)frame_paddr,  PDE2vaddr(pml4t_idx,pdpt_idx,pd_idx), PG_RW | PG_USER);
	framearray[frame_paddr/PAGE_SIZE].vaddr = PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0);
      	kmemset(PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0), 0, PAGE_SIZE);

      }
    }//pt's 

    /** finally attach the frames to actual pt's */
    pt = (struct page_table *)PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,0);
    if( !(pt->entries[pt_idx].present) ) {
      frame_paddr = get_free_frame();
      setup_table((void*)frame_paddr, PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx), flags);
      framearray[frame_paddr/PAGE_SIZE].vaddr = idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx);
      kmemset(idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx), 0, PAGE_SIZE);
    }

    pt_idx++;
  }  //for loop on framearray_pages

  return;
}


#define VADDR2PTE(vaddr) \
  ((union page*)PTE2vaddr(virt2pml4t(vaddr), virt2pdpt(vaddr), \
                          virt2pd(vaddr), virt2pt(vaddr)))

void set_page_permission(uint64_t vaddr, uint64_t length, uint64_t flags) {

  uint64_t end;

  end = vaddr + length;

  /* round vaddr down to the page boundary. */
  vaddr -= vaddr % PAGE_SIZE;

  for ( ; vaddr <= end; vaddr += PAGE_SIZE ) {
    VADDR2PTE(vaddr)->rw     = ( flags & PG_RW )     ? 1 : 0;
    VADDR2PTE(vaddr)->nx     = ( flags & PG_NX )     ? 1 : 0;
    VADDR2PTE(vaddr)->us     = ( flags & PG_USER )   ? 1 : 0;
    VADDR2PTE(vaddr)->global = ( flags & PG_GLOBAL ) ? 1 : 0;
  }

  return;
}
int is_vaddr_mapped(uint64_t* addr) {

  int pml4t_idx, pdpt_idx, pd_idx, pt_idx;

  pml4t_idx = virt2pml4t(addr);
  pdpt_idx = virt2pdpt(addr);
  pd_idx = virt2pd(addr);
  pt_idx = virt2pt(addr);

  if ( !((union pt_entry*)PML4TE2vaddr(pml4t_idx))->present )
    return 0;
  
  if ( !((union pt_entry*)PDPTE2vaddr(pml4t_idx, pdpt_idx))->present )
    return 0;
  
  if ( !((union pt_entry*)PDE2vaddr(pml4t_idx, pdpt_idx, pd_idx))->present )
    return 0;	

  if ( !((union pt_entry*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->present)
    return 0;

  return 1;
}

void vmem_free(uint64_t* base, uint64_t length) {
  vmem_free_imp(base, length, 1);
}

void vmem_free_temp(uint64_t* base, uint64_t length) {
  vmem_free_imp(base, length, 0);
}

void vmem_free_imp(uint64_t* base, uint64_t length, int free) {

  uint64_t j;
  uint64_t paddr, pte_vaddr;
  
  int inc_pdpt_idx, inc_pml4t_idx;
  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;

  pml4t_idx = virt2pml4t(base);
  pdpt_idx = virt2pdpt(base);
  pd_idx = virt2pd(base);
  pt_idx = virt2pt(base);

  /** first loop - free frames but not the paging structs */
  while ( (uint64_t)idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx) < ((uint64_t)base+length) ) {

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
            kputs("vmem_free range beyond virtual memory address space");
            panic();
          }
	}//pdpt's
      }//pd's
    }//pt's 

    /* if we rolled over the indeces above, the condition tested in the 
       while loop wasn't an accurate test of what we're trying to do */
    if ( !((uint64_t)idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx) < ((uint64_t)base+length)) ) 
      break;


    /** detach the frame from actual pte */
    pte_vaddr = (uint64_t)PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx);
    if ( PTE_is_present(pml4t_idx, pdpt_idx, pd_idx, pt_idx) ) {
      if ( free ) {
	kmemset(idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx), 0, PAGE_SIZE);
	paddr = TABLE2ADDR(((union page*)pte_vaddr)->addr);

	framearray[paddr/PAGE_SIZE].vaddr = 0x0;
	framearray[paddr/PAGE_SIZE].free  = 0x1;
	framearray[paddr/PAGE_SIZE].proc  = 0x0;

      }
      *(uint64_t*)pte_vaddr = 0x0;
    }
    
    pt_idx++;
  }//for loop

  pml4t_idx = virt2pml4t(base);
  pdpt_idx = virt2pdpt(base);
  pd_idx = virt2pd(base);

  /* second loop - free structures if necessary */
  while ( 1 ) {

    if ( (uint64_t)idx2vaddr(pml4t_idx,pdpt_idx,pd_idx,0) >= (uint64_t)base + length )
      break;

    inc_pdpt_idx = 0;
    inc_pml4t_idx = 0;

    /** potentially detatch pt from the pdt */
    for ( j = 0; j < 512; j++ ) {
      if ( PTE_is_present(pml4t_idx, pdpt_idx, pd_idx, j) )
	break;
    }

    if ( j == 512 && PDE_is_present(pml4t_idx, pdpt_idx, pd_idx) ){
      kmemset(PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0), 0, PAGE_SIZE);
      paddr = TABLE2ADDR(((union pt_entry*)PDE2vaddr(pml4t_idx, pdpt_idx, pd_idx))->addr);

      framearray[paddr/PAGE_SIZE].vaddr = 0x0;
      framearray[paddr/PAGE_SIZE].free  = 0x1;
      framearray[paddr/PAGE_SIZE].proc  = 0x0;

      *(uint64_t*)PDE2vaddr(pml4t_idx, pdpt_idx, pd_idx) = 0x0;
    }
    /* whether I freed the PT or not, I will need to move to the next PT bc 
       I will make the same decision for any pt_idx within this PT */
    if ( ++pd_idx == 512 ) {
      inc_pdpt_idx = 1;
      pd_idx = 0;
    }

    /** potentially detach pdt from the pdpt */    
    for ( j = 0; j < 512; j++ ) {
      if ( PDE_is_present(pml4t_idx, pdpt_idx, j) )
	break;
    }

    if ( j == 512 && PDPTE_is_present(pml4t_idx, pdpt_idx) ) {
      kmemset(PDE2vaddr(pml4t_idx, pdpt_idx, 0), 0, PAGE_SIZE);
      paddr = TABLE2ADDR(((union pt_entry*)PDPTE2vaddr(pml4t_idx, pdpt_idx))->addr);

      framearray[paddr/PAGE_SIZE].vaddr = 0x0;
      framearray[paddr/PAGE_SIZE].free  = 0x1;
      framearray[paddr/PAGE_SIZE].proc  = 0x0;

      *(uint64_t*)PDPTE2vaddr(pml4t_idx, pdpt_idx) = 0x0;
      
      inc_pdpt_idx = 1;
    }

    /** potentially delete pdpt from the pml4t **/         
    for ( j = 0; j < 512; j++ ) {
      if ( PDPTE_is_present(pml4t_idx, j) )
	break;
    }

    if ( j == 512 && PML4TE_is_present(pml4t_idx) ) {
      kmemset(PDPTE2vaddr(pml4t_idx, 0), 0, PAGE_SIZE);
      paddr = TABLE2ADDR(((union pt_entry*)PML4TE2vaddr(pml4t_idx))->addr);

      framearray[paddr/PAGE_SIZE].vaddr = 0x0;
      framearray[paddr/PAGE_SIZE].free  = 0x1;
      framearray[paddr/PAGE_SIZE].proc  = 0x0;

      *(uint64_t*)PML4TE2vaddr(pml4t_idx) = 0x0;

      inc_pml4t_idx = 1;
    }

    if ( inc_pdpt_idx ) 
      if ( ++pdpt_idx == 512 )
	pdpt_idx = 0;

    if ( inc_pml4t_idx || (inc_pdpt_idx && (pdpt_idx == 0)) ) {
      if ( ++pml4t_idx == 512 ) {
	break;
      }
    }

  }

  /* TODO: fix this annoying bug that we cant find ): */
  save_frame_array_idx = 0;
  
  return;
}

void vmem_init(void) {

#ifdef DIVERSITY
  uint64_t rand;
#endif
  struct allocated_chunk *heap_chunk;

  /* persistant frame array index to speed up get_free_frame search */
  save_frame_array_idx = 0;

#ifdef DIVERSITY
  rand = random_between(1,510);
  
#ifdef DEBUG
  kprintf("[Boot2] Heap PML4T IDX =  %d\n", rand);
#endif

  frame_array_vaddr = (uint64_t)idx2vaddr( rand, 0, 0, 0 );
#else
  frame_array_vaddr = (uint64_t)idx2vaddr( 12, 0, 0, 0 );
#endif
  if ( frame_array_vaddr % PAGE_SIZE )
    frame_array_vaddr += PAGE_SIZE - ( frame_array_vaddr % PAGE_SIZE );
  framearray = (struct frame_array_entry_t*)frame_array_vaddr;
  
  /* build the frame array */
  frame_array_init();
#ifdef DEBUG
  kprintf("[VMEM_LAYER] Initialized the frame array: vaddr 0x%x, len 0x%x\n", frame_array_vaddr, frame_array_len);
#endif

  /* put the heap right after the frame array (but on a page boundary) */
  heap_start = (uint64_t)(frame_array_vaddr + (frame_array_len*sizeof(struct frame_array_entry_t)));
  heap_start += PAGE_SIZE - (heap_start % PAGE_SIZE);

  /* NOW WE CAN USE VMEM_ALLOC!! hooray */
  vmem_alloc((uint64_t*)heap_start, KHEAP_SIZE, PG_NX | PG_RW | PG_GLOBAL);

#ifdef DEBUG
  print_name();
  kprintf("Initializing small-structure heap...\n");
  vmem_print_pages(1);
  print_name();
  kprintf("Heap Start %x HEX\n", heap_start);
  print_name();
  kprintf("Heap Size %x HEX\n", KHEAP_SIZE);
#endif

  kheapinit((uint64_t*)heap_start, KHEAP_SIZE / sizeof(uint64_t));
  heap_ready = 1;

#ifdef DEBUG
  vmem_print_pages(1);
  print_name();
  kprintf("Small structure heap (KMALLOC) initsialized. \n");
#endif

  heap_chunk = kmalloc_track(VMEM_LAYER_SITE,sizeof(struct allocated_chunk));
  heap_chunk->base = (uint64_t)heap_start;
  heap_chunk->size = KHEAP_SIZE;
  heap_chunk->pid  = 0;

  return;
}

void seed_vmem_layer( uint64_t frame_array_address, uint64_t lheap_start ) {

  struct memmap *chunk = (struct memmap *)MEMORY_MAP;
  uint16_t *numchunks = (uint16_t *)MEMORY_MAP_ENTRIES;

  /* set the address of the frame array - it's been initialized by boot2 */
  frame_array_vaddr = frame_array_address; 
  framearray = (struct frame_array_entry_t*)frame_array_address;

  frame_array_len = chunk[*numchunks - 1].base + chunk[*numchunks - 1].length;
  frame_array_len /= PAGE_SIZE;

  heap_start = lheap_start;

  kheapinit((uint64_t*)heap_start, KHEAP_SIZE / sizeof(uint64_t));

  return;
}

uint64_t virt2phys(void *vaddr) {

  uint64_t pml4t_idx, pdpt_idx, pd_idx, pt_idx;

  /** figure out where in the tables we will store the array based on the 
      fixed virtual address given **/
  pml4t_idx = virt2pml4t(vaddr);
  pdpt_idx = virt2pdpt(vaddr);
  pd_idx = virt2pd(vaddr);
  pt_idx = virt2pt(vaddr);

  return TABLE2ADDR(((union pt_entry*)PTE2vaddr(pml4t_idx,pdpt_idx,pd_idx,pt_idx))->addr);
}

void *phys2virt(uint64_t physical) {

  return framearray[physical / PAGE_SIZE].vaddr;
}

inline void __invlpg(uint64_t addr){
  asm volatile ("invlpg (%0)"	::"r" (addr): "memory" );
}

/* Functions for invalidating pages when we monkey with the page tables. */
void reload_cr3() {
  write_cr3(read_cr3());
}

/*******************************************************************************
 *
 * Function: new_stack
 *
 * Description: Returns a properly 16-byte-aligned stack. If not NULL, the first
 *              argument will be filled in with the address of the bottom of
 *              the stack.  This is so that it can be properly freed later.
 *
 ******************************************************************************/
uint8_t *new_stack(uint8_t **stack_top) {
  uint64_t s = (uint64_t)kmalloc_track(VMEM_LAYER_SITE, 
				       KSTACK_PAGES*PAGE_SIZE);
  if(stack_top != NULL)
    *stack_top = (uint8_t *)s;
  s += KSTACK_PAGES*PAGE_SIZE;
  return (uint8_t *)s;
}

uint64_t get_frame_array_vaddr( void ) {
  return frame_array_vaddr;
}

uint64_t get_heap_start( void ) {
  return heap_start;
}
