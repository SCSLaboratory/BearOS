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

/******************************************************************************
 * Filename: kvmem.c
 *
 * Description: Contains all the functions for managing virtual memory. This
 *              includes adding/removing pages, allocating/deallocating paging
 *              structures, and managing the memory-mapped I/O (MMIO) segment
 *              of the virtual address space.
 *
 *****************************************************************************/

#include <constants.h>
#include <kstring.h>        /* For kmemset kmemcpy */
#include <stdint.h>         /* For std types     */
#include <memory.h>         /* For stuff         */
#include <kstdio.h>         /* For kprintf        */
#include <kqueue.h>         /* For queues        */
#include <kmalloc.h>        /* For malloc/free   */
#include <sbin/vgad.h>      /* For vga_set_loc() */
#include <proc.h>           /* For pid_t         */
#include <kvcall.h>         /* For VT-d hack     */
#include <khash.h>          /* For hash table    */
#include <kvmem.h>          /* For derp          */
#include <asm_subroutines.h>

#define KVMEM_TYPE_MMIO 0
#define KVMEM_TYPE_DMA  1

typedef struct {
  int type;
  pid_t pid;
  uint64_t vaddr;
  uint64_t paddr;
  unsigned int num_pages;
} Mem_alloc_t;

static void *alloc_table; /* Hash table of driver memory allocations by pid */
#define ALLOC_TABLE_SLOTS 9

/******************************************************************************
 **************************** PRIVATE FUNCTIONS *******************************
 *****************************************************************************/

static void kvmem_add_dev_alloc(pid_t pid, int type, uint64_t vaddr, 
				uint64_t paddr, int num_pages);
static int  kvmem_matches_pid  (void *elementp, const void *searchkeyp);

/******************************************************************************
 *
 * Function: kvmem_init
 *
 * Description: Initializes virtual memory for the kernel
 *
 *****************************************************************************/
void kvmem_init( uint64_t frame_array_address, uint64_t heap_start ) {
  uint64_t vaddr, paddr, end_addr;

  /* Initialize virtual memory layer */
  seed_vmem_layer( frame_array_address, heap_start );

  /*
   * VGA memory has already been mapped into higher half.  Point the VGA 
   * driver there before the lower virtual memory mapping gets blown away.
   */
  vga_set_loc(LEGACY_VGA_MEM);

  /* 
   * Now set the VGA memory as uncacheable.  The MTRR's will catch this, but
   * when we're running with the hypervisors the MTRRs aren't used.
   */
  vaddr = (uint64_t)LEGACY_VGA_MEM;
  paddr = (uint64_t)PHYS_VGA_MEM;
  end_addr = (uint64_t)(LEGACY_VGA_MEM + VGA_MEM_SIZE);
  for( ; vaddr < end_addr; vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
    attach_page(vaddr, paddr, KMEM_IO_FLAGS);
  }

#ifdef DEBUG
  kputs("[KERNEL] Mapped VGA memory");
#endif

  /* Initialize driver memory allocation table. */
  alloc_table = hopen(ALLOC_TABLE_SLOTS);
  if (alloc_table == NULL)
    kputs("ERROR: Could not allocate space for driver mem alloc table");

  return;
}

/******************************************************************************
 ********************* MANIPULATION OF PAGING STRUCTURES **********************
 *****************************************************************************/

/******************************************************************************
 *
 * Function: attach_page
 *
 * Description: Given a page-aligned virtual address and a page-aligned
 *              physical address, this function modifies the currently-mapped
 *              page tables by adding the phsyical page in at the given virtual
 *              address.  If there is already a page there, the page table
 *              structures are overwritten.
 *
 * Arguments:
 *   vaddr: The virtual address at which to stick the page.
 *   paddr: The physical address of the page going in.
 *   ptflags: The flags put into the PML4T, PDPT, and PD entries.
 *   pgflags: The flags put into the page table entry.
 *
 *  See procman.h for a list of page table flags (FIXME: This should probably 
 * go into memory.h)
 *
 *****************************************************************************/
/* TODO: this dulicate copy of the attach_page function is designed simply to 
   not adjust the frame array... it exists to help old kernel contexts catch 
   up with what the new kernel core kernel context does. When there is jsut 1 
   kernel context, this can go away. */
int attach_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
  int pml4t_idx, pdpt_idx, pd_idx, pt_idx;

  /* Check that addresses are page-aligned */
  if (vaddr & 0xFFF){
    kprintf("attempt to attach unaligned virt address 0x%x\n", vaddr);
    return -1;
  }

  /* Check that addresses are page-aligned */
  if (paddr & 0xFFF){
    kprintf("attempt to attach unaligned phys address 0x%x\n", paddr);
    return -1;
  }

  vaddr_init((uint64_t*)vaddr, flags);

  pml4t_idx = virt2pml4t(vaddr);
  pdpt_idx = virt2pdpt(vaddr);
  pd_idx = virt2pd(vaddr);
  pt_idx = virt2pt(vaddr);

  setup_table((void*)paddr, PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx), flags);
  framearray[paddr/PAGE_SIZE].vaddr = idx2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx);

  return 0;
}

/*
 * Does not attempt to free the page that gets unmapped; this function does not
 * know if that needs to happen or not.
 * Does not attempt to clean up paging structures; this is done on exit.
 */
void detach_page(Proc_t *p, uint64_t vaddr) {
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;
  union page *page;
  int pdpt_idx, pd_idx, pt_idx;

  /* In user proc address space? */
  if (vaddr > ((uint64_t)(1) << 39))
    return;

  /* Check that address is page-aligned */
  if (vaddr & 0xFFF)
    return;

  /* Get indices needed to find the relevant page table entry. */
  pdpt_idx  = virt2pdpt (vaddr);
  pd_idx    = virt2pd   (vaddr);
  pt_idx    = virt2pt   (vaddr);

  /* Go find that pte. */
  /* FIXME: Need some sanity checking here */
  pdpt = (struct page_directory_pointer_table *)(phys2virt(TABLE2ADDR(p->pml4t_entry->addr)));
  pd = (struct page_directory *)(phys2virt(TABLE2ADDR((pdpt->entries[pdpt_idx]).addr)));
  pt = (struct page_table *)(phys2virt(TABLE2ADDR((pd->entries[pd_idx]).addr)));
  page = &(pt->entries[pt_idx]);

  /* Zero out that pte. */
  kmemset(page, 0, sizeof(union page));
}

/******************************************************************************
 *
 * Function: flush_tlb(int)
 *
 * Description: Flushes the given part of the address space from the TLB.
 *
 *****************************************************************************/
void flush_tlb(int where) {
  switch(where) {
    uint64_t cr3;
  case TLB_PL0:
  case TLB_PL1:
  case TLB_PL2:
  case TLB_PL3:
  case TLB_SCRATCH:
  case TLB_ALL:
  default:
    cr3 = read_cr3();
    write_cr3(cr3);
    break;
  }
}

/******************************************************************************
 ******************* PERIPHERAL DEVICE MEMORY MANAGEMENT **********************
 *****************************************************************************/

/******************************************************************************
 *
 * Function: map_io_mem
 *
 * Description: Given a phys addr and size in bytes, it will map the memory 
 *              into the kernel and return the virtual address of the new 
 *              memory region.
 *
 *****************************************************************************/
uint64_t kvmem_map_mmio(Proc_t *p, uint64_t virt_addr, Pci_bar_t *bar) {
  uint64_t phys_addr;
  uint64_t max_paddr;

  phys_addr = bar->bar_base;
  max_paddr = bar->bar_base + bar->bar_size; /* DO NOT USE bar_limit */

  if (virt_addr & 0xFFF)
    return -1;

  /* 
   * Add this allocation to the allocation table, so we can take care of it
   * nicely when the process ends. 
   */
  kvmem_add_dev_alloc(p->pid, KVMEM_TYPE_MMIO, virt_addr, bar->bar_base, 
		      bar->bar_size / PAGE_SIZE);

  for (;
       phys_addr < max_paddr;
       phys_addr += PAGE_SIZE, virt_addr += PAGE_SIZE) {
    attach_page(virt_addr, phys_addr, UMEM_IO_FLAGS_UC);
  }
  /*TODO switch to void when speicalk exists */
  /* Return the next available address so we know where mmio ends. */
  return virt_addr;
}

uint64_t kvmem_map_dma_region(Proc_t *p, uint64_t virt_addr, uint64_t bytes) {

  uint64_t phys_addr;
  int i;

  /* Map in the memory */
  phys_addr = get_contiguous_frames(bytes);

  for ( i = 0; i < bytes/PAGE_SIZE; i++, virt_addr += PAGE_SIZE ) 
    attach_page(virt_addr, phys_addr + i*PAGE_SIZE, UMEM_IO_FLAGS_UC);

  /* Return the starting physical address. */
  return phys_addr;
}

/* TODO: FIX with new system */
void kvmem_unmap_devmem(Proc_t *p) {
  Mem_alloc_t *dev_alloc;

  while ((dev_alloc = (Mem_alloc_t *)hremove(alloc_table, kvmem_matches_pid, 
					     ((const char*)(&(p->pid))), sizeof(pid_t))) != NULL) {
    int i;

    /* Debug */
#ifdef DEBUG
    kprintf("	[KVmem] Unmapped dev memory*type=%d vaddr=0x%x paddr=0x%x*", dev_alloc->type, dev_alloc->vaddr,
	    dev_alloc->paddr);
#endif 
    /* Unmap. */
    for(i=0; i<(dev_alloc->num_pages); i++) {
      uint64_t vaddr;
      vaddr = dev_alloc->vaddr + (uint64_t)(i) * (uint64_t)(PAGE_SIZE);
      detach_page(p, vaddr);
    }

    /* Free the allocation record. */
    kfree_track(KVMEM_SITE,dev_alloc);
  }
}

/************************* PRIVATE FUNCTIONS *********************************/

static void kvmem_add_dev_alloc(pid_t pid, int type, uint64_t vaddr, 
				uint64_t paddr, int num_pages) {
  Mem_alloc_t *alloc;
	
  alloc = kmalloc_track(KVMEM_SITE,sizeof(Mem_alloc_t));
  if (alloc == NULL) {
    kprintf("%s:%d :: Could not allocate an allocation object\n", __FILE__, __LINE__);
    return;
  }

  alloc->pid = pid;
  alloc->type = type;
  alloc->vaddr = vaddr;
  alloc->paddr = paddr;
  alloc->num_pages = num_pages;

  hput(alloc_table, alloc, (const char *)(&(alloc->pid)), sizeof(pid_t));
}

static int kvmem_matches_pid(void *elementp, const void *searchkeyp) {

  return ((Mem_alloc_t*)elementp)->pid == *(pid_t*)searchkeyp;
}
