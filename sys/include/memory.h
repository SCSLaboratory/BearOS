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

#pragma once
#include <stdint.h>
#include <kvmem_sites.h>
#include <vk.h>

/* ---------------------------------------------------------------------------
 * ----------------------------- CONSTANTS -----------------------------------
 * ---------------------------------------------------------------------------
 */

/* Amount of memory allocated to every guest. */
#define GUEST_MEM_SIZE 0x10000000UL

/********************* KERNEL MEMORY MAP ***********************/

/* 
 * Text of process binaries begins here (including drivers but not systask) 
 * 0x8000 = 32KB
 */
#define USR_TEXT_START 0x8000

/* 
 * Beginning of MMIO/DMA space for drivers. 
 * So driver binaries can only be (DRIVER_MEM_START - USR_TEXT_START) bytes.
 * 0x1400000 = 20MB
 */
#define DRIVER_MEM_START (0x80000000000UL)

/* Size of stack */
#define USR_STACK_PAGES 8 

/*
 * Location of the variable that holds the location of the user space binary
 * This needs to be passed to the ELF loader
 */
#define BINARY_LOCATION	0xb00b

/* size of the kernel heap */
#define KHEAP_SIZE 0x4000000

#define EPT_TYPE_CACHEABLE_WB  6
#define EPT_TYPE_CACHEABLE_WT 4
#define EPT_TYPE_UNCACHEABLE 0

#define PML4T_RECURSIVE_ENTRY_IDX 511

/**************** END KERNEL MEMORY MAP ***********************/

/* Put a physical address in the "addr" slot of the table.
 * This is equivalent to setting table->bits to the actual address, by the way,
 * so long as you set table->bits first.
 */
#define ADDR2TABLE(addr) ((uint64_t)(addr) >> 12)
/* And vice versa */
#define TABLE2ADDR(addr) (((uint64_t)(addr)) << 12)

/* These macros take a virtual address, and return the parts used for indexing
 * into the various page table levels.
 */
#define virt2pml4t(addr) ((((uint64_t)(addr)) >> 39) & 0x1FF)
#define virt2pdpt(addr)  ((((uint64_t)(addr)) >> 30) & 0x1FF)
#define virt2pd(addr)    ((((uint64_t)(addr)) >> 21) & 0x1FF)
#define virt2pt(addr)    ((((uint64_t)(addr)) >> 12) & 0x1FF)
#define virt2pg(addr)    ((((uint64_t)(addr))      ) & 0xFFF)

/******************************************************************************
 ************************** PAGING PROTECTION BITS ****************************
 *****************************************************************************/

/* 
 * These flags are used by set_pt_flags() to tell which bits to set in PT
 * entries. Note that these may not correspond to the actual bit locations in
 * the entries; in fact, the flag layout for the PT is DIFFERENT from that of
 * the PML4T, PDPT, and PD.  The structures in memory.h define what bits 
 * actually get wiggled.
 */
/*     FLAG NAME       */   /*  ACTION WHEN SET                              */
#define PG_PRESENT    0x1   /* Page is accessible.                           */
#define PG_RW         0x2   /* Page can be read and written.                 */
#define PG_USER       0x4   /* Page is accessible to user processes.         */
#define PG_PWT       0x10   /* Must be set for uncached (MMIO/DMA) memory.   */
#define PG_PCD       0x20   /* Must be set for uncached (MMIO/DMA) memory.   */
#define PG_ACCESSED  0x40   /* Page has been accessed since bit last cleared */
#define PG_DIRTY    0x100   /* Page has been modified since bit last cleared */
#define PG_PAT      0x200   /* Reserved (not implemented)                    */
#define PG_GLOBAL   0x400   /* Reserved (not implemented)                    */
#define PG_NX       0x800   /* Page cannot be executed.                      */

/* Permissions for user process text */
#define USR_TEXT_PERMS (PG_PRESENT | PG_RW | PG_USER)

/* Permissions for user process stack */
#define USR_STACK_PERMS (PG_PRESENT | PG_RW | PG_NX | PG_USER)

/* Permissions for peripheral device memory regions in kernel. */
#define KMEM_IO_FLAGS    (PG_PRESENT | PG_RW | PG_GLOBAL )
#define KMEM_IO_FLAGS_UC (PG_PRESENT | PG_RW | PG_NX | PG_PWT | PG_PCD | PG_GLOBAL )

/* Permissions for peripheral device memory regions in user processes. */
#define UMEM_IO_FLAGS    (PG_PRESENT | PG_RW | PG_USER)
#define UMEM_IO_FLAGS_UC (PG_PRESENT | PG_RW | PG_NX | PG_USER | PG_PWT | PG_PCD)


/** These four functions make use of the self-referencing page table trick 
    to build a mapping to a given paging structure entry; either a pml4t entry,
    a pdpt entry, a pd entry, or a pt entry. */
#define PTE2vaddr(pml4te, pdpte, pde, pte) ((uint64_t*)(0xffffff8000000000 | (((uint64_t)((pml4te) & 0x1ff)) << 30) \
							| (((uint64_t)((pdpte) & 0x1ff)) << 21) \
							| (((uint64_t)((pde) & 0x1ff)) << 12) \
							| (((uint64_t)((pte) & 0xfff))*8)))

#define PDE2vaddr(pml4te, pdpte, pde)      ((uint64_t*)(0xffffffffc0000000 | (((uint64_t)((pml4te) & 0x1ff)) << 21) \
							| (((uint64_t)((pdpte) & 0x1ff)) << 12) \
							| (((uint64_t)((pde) & 0xfff)) * 8)))

#define PDPTE2vaddr(pml4te,pdpte)          ((uint64_t*)(0xffffffffffe00000 | (((uint64_t)((pml4te) & 0x1ff)) << 12) \
							| (((uint64_t)((pdpte) & 0xfff)) * 8)))

#define PML4TE2vaddr(pml4te)               ((uint64_t*)(0xfffffffffffff000 | (((uint64_t)((pml4te) & 0xfff)) * 8)))  

#define idx2vaddr(pml4te, pdpte, pde, pte) ((uint64_t*)(		\
							(((uint64_t)((pml4te) & 0x1ff)) << 39) | \
							(((uint64_t)((pml4te) & 0x100 ? 0xffff : 0x0)) << 48) | \
							(((uint64_t)((pdpte) & 0x1ff)) << 30) | \
							(((uint64_t)((pde) & 0x1ff)) << 21)   | \
							(((uint64_t)((pte) & 0x1ff)) << 12)))

#define PTE_is_present(pml4te, pdpte, pde, pte) (is_vaddr_mapped(PTE2vaddr((pml4te), (pdpte), (pde), (pte))) \
						 && ((union pt_entry*)PTE2vaddr((pml4te), (pdpte), (pde), (pte)))->present)

#define PDE_is_present(pml4te, pdpte, pde) (is_vaddr_mapped(PDE2vaddr((pml4te), (pdpte), (pde))) && ((union pt_entry*)PDE2vaddr((pml4te), (pdpte), (pde)))->present)

#define PDPTE_is_present(pml4te, pdpte) (is_vaddr_mapped(PDPTE2vaddr((pml4te), (pdpte))) && ((union pt_entry*)PDPTE2vaddr((pml4te), (pdpte)))->present)

#define PML4TE_is_present(pml4te) (is_vaddr_mapped(PML4TE2vaddr((pml4te))) && ((union pt_entry*)PML4TE2vaddr((pml4te)))->present)

/* variable to hold the (possibly diversified) base of the kernel stack */
uint64_t system_stack_base;

#ifndef BOOTLOADER
struct frame_array_entry_t *framearray;
#endif

/* ---------------------------------------------------------------------------
 * ----------------------------- STRUCTURES ----------------------------------
 * ---------------------------------------------------------------------------
 */

/* Memory map provided by the BIOS. */
struct memmap {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t acpi30;                         /* Not portable, don't touch. */
} __attribute__ ((packed));

struct frame_array_entry_t{
  uint64_t *vaddr;
  uint16_t free;
  uint16_t type;
  uint32_t proc;
} __attribute__ ((packed));

/* Layout of an entry in the page table. */
union page {
  uint64_t bits;                           /* Full page entry */
  struct {                                 /* Individual fields */
    uint64_t present:1;                  /* Pagefaults if 0 */
    uint64_t rw:1;                       /* Read/Write */
    uint64_t us:1;                       /* User/Supervisor */
    uint64_t pwt:1;                      /* Page-level write through */
    uint64_t pcd:1;                      /* Page-level cache disable */
    uint64_t accessed:1;
    uint64_t dirty:1;
    uint64_t pat:1;
    uint64_t global:1;
    uint64_t ignored2:3;
    uint64_t addr:40;                    /* Physical address (4kb align) */
    uint64_t ignored1:11;
    uint64_t nx:1;                       /* No-execute bit */
  } __attribute__ ((packed));
};

union ept_page {
  uint64_t bits;
  struct {
    uint64_t r:1;
    uint64_t w:1;
    uint64_t x:1;
    uint64_t type:3;
    uint64_t ign_pat:1;
    uint64_t ignored1:5;
    uint64_t addr:40;
    uint64_t ignored2:12;
  } __attribute__ ((packed));
};

/* Layout of an entry in the page directory. */
union pt_entry {
  uint64_t bits;                           /* Full page table entry */
  struct {                                 /* Individual fields */
    uint64_t present:1;                  /* Pagefaults if 0 */
    uint64_t rw:1;                       /* Read/Write */
    uint64_t us:1;                       /* User/Supervisor */
    uint64_t pwt:1;                      /* Page-level write through */
    uint64_t pcd:1;                      /* Page-level cache disable */
    uint64_t accessed:1;
    uint64_t ignored3:1;
    uint64_t ps:1;                       /* Page size (zero) */
    uint64_t ignored2:4;
    uint64_t addr:40;                    /* Physical address (4kb align) */
    uint64_t ignored1:11;
    uint64_t nx:1;                       /* No-execute bit */
  } __attribute__ ((packed));
};

union ept_pt_entry {
  uint64_t bits;
  struct {
    uint64_t r:1;
    uint64_t w:1;
    uint64_t x:1;
    uint64_t reserved1:5;
    uint64_t ignored1:4;
    uint64_t addr:40;
    uint64_t ignored2:12;
  } __attribute__ ((packed));
};


/* Page table structures. Note that we have some "pseudo-OO" here -- each
 * structure can be used for either regular page table entries, OR EPT page
 * table entries. This does _not_ mean that a structure contains both regular
 * page table info and EPT page table info -- it's one or the other (despite 
 * the use of union). We do it this way to avoid duplicating code that we 
 * shouldn't really have to.
 */
struct page_table {
  union {
    union page entries[512];
#ifdef HYPV
    union ept_page ept_entries[512];
#endif
  };
};

struct page_directory {
  union {
    union pt_entry entries[512];
#ifdef HYPV
    union ept_pt_entry ept_entries[512];
#endif
  };
};

struct page_directory_pointer_table {
  union {
    union pt_entry entries[512];
#ifdef HYPV
    union ept_pt_entry ept_entries[512];
#endif
  };
};

struct page_map_level_4_table {
  union {
    union pt_entry entries[512];
#ifdef HYPV
    union ept_pt_entry ept_entries[512];
#endif
  };
};

/*
 * VK module 
 * A pointer to the VK heap, to be initialized via vkinit()
 * Number of pages in the heap
 */
vkheap_t *vk_heap;

#define VK_HEAP_NUM_PAGES 5000


/* ----------------------------------------------------------------------
 * ----------------------------- PUBLIC FUNCTIONS -----------------------
 * ----------------------------------------------------------------------
 */

/* Exactly what it says on the tin. Only works with the kernel 
   address space though, because Intel doesn't support arbitrarily 
   driving the MMU.
 */
uint64_t virt2phys(void *);
void *phys2virt(uint64_t);


/* Invalidate pages for when we change the page tables. */
void reload_cr3();
inline void __invlpg(uint64_t addr);

/* Functions that affect the vmem layer. */
void vmem_init(void);
int is_region_mapped(void *base, size_t size);

/*functions for debugging and testing memory */
void vmem_print_pages(int alloc);
void print_alloc_q();

void setup_table( void *phys, void *vaddr, uint64_t flags);

/* allocate new stack w/ vmem_alloc_region */
uint8_t *new_stack(uint8_t **);    

void vaddr_init(uint64_t *vaddr, uint64_t flags);

uint64_t get_flags( union page *p );

inline void cache_flush(volatile void *addr);

void vmem_alloc( uint64_t* base, uint64_t length, uint64_t flags );
void vmem_free(uint64_t* base, uint64_t length);
void vmem_free_temp(uint64_t* base, uint64_t length);
uint64_t get_contiguous_frames(uint64_t length);
uint64_t get_frame_array_vaddr( void );
uint64_t get_heap_start( void );
void seed_vmem_layer( uint64_t frame_array_address, uint64_t heap_start );
uint64_t get_free_frame(void);
void set_page_permission(uint64_t vaddr, uint64_t length, uint64_t flags);

