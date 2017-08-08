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

#include <asm_subroutines.h>
#include <constants.h>
#include <vmx.h>
#include <kstdio.h>
#include <kstring.h>
#include <ept.h>
#include <memory.h>
#include <acpi.h>
#include <ramio.h>
#include <vk.h>
#include <kvmem.h>
#include <vmx_utils.h>

extern struct RSDPDescriptor20 rsdpdesc;
static int first = 1;

#define VIRT    1
#define PHYS    0
#define CACHE   1
#define UNCACHE 0

/* Create the EPTs for the "guest-physical" memory, i.e., translate guest-
 * physical addresses into actual-physical addresses. We used to just create a
 * mirror of the system page tables for doing this and have the guest translate
 * into hypervisor-linear addresses, but this caused protection problems and
 * violated some assumptions we made about memory as a guest. So now we just
 * give each guest its own set of tables. As an added bonus, it should make it
 * easier to expand guest memory in the future. Yay.
 *
 * Initial permissions are permissive -- info given by the ELF loader will
 * further restrict them.
 */
#define init_entry(entry, address, virt) {		                      \
    (entry)->r = 1;                                                           \
    (entry)->w = 1;                                                           \
    (entry)->x = 1;                                                           \
    (entry)->addr = ADDR2TABLE(virt ? virt2phys((void *)(address)) : (uint64_t)address); \
  }

#define init_ept_page(entry, address, virt, cache) {			      \
    (entry)->r    = 1;							      \
    (entry)->w    = 1;							      \
    (entry)->x    = 1;	 						      \
    (entry)->type = cache ? EPT_TYPE_CACHEABLE_WB : EPT_TYPE_UNCACHEABLE;     \
    (entry)->addr = ADDR2TABLE(virt ? virt2phys((void *)(address)) : address);\
  }

void create_ept(vproc_t *vp) {
  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;
  union ept_pt_entry *entry;
  union ept_page *page;
  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;
  uint32_t i;
  uint64_t addr, vaddr, paddr, j;
  int length;
  struct RSDPDescriptor20* guest_rsdp;
  uint32_t acpi_start, acpi_end;
  struct memmap *chunk = (struct memmap *)MEMORY_MAP;
  uint16_t *numchunks = (uint16_t *)MEMORY_MAP_ENTRIES;

  uint32_t *entry_ptr, *end_rsdt;

  /* Create the PML4T to start us off */
  /* Get an available virtual address */
  vaddr = vkmalloc(vk_heap, 1);
 
  vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, PG_RW);
  pml4t = (struct page_map_level_4_table *)vaddr;
  kmemset(pml4t, 0, PAGE_SIZE);

#ifdef DEBUG
  kprintf("     [vproc] Creating EPT tables pml4t = 0x%x\n", pml4t);
#endif

  /* Set initial values so that the first elements of the page-table chain
   * will be created upon loop entry. */
  pml4t_idx = -1;
  pdpt_idx = 511;
  pd_idx = 511;
  pt_idx = 512;
  for(j = 0; j < vp->memsz; j += PAGE_SIZE) {
    if(pt_idx == 512) {
      pd_idx++;
      pt_idx = 0;

      if(pd_idx == 512) {
        pdpt_idx++;
        pd_idx = 0;

        if(pdpt_idx == 512) {
          pml4t_idx++;
          pdpt_idx = 0;

          if(pml4t_idx == 1)
            panic("Overran the PML4T while creating EPTs!");

          /* Create a new PDPT, set up the next PML4T entry. */
	  vaddr = vkmalloc(vk_heap, 1);
          vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
          pdpt = (struct page_directory_pointer_table *)vaddr;
          kmemset(pdpt, 0, PAGE_SIZE);

          entry = &pml4t->ept_entries[pml4t_idx];
          init_entry(entry, pdpt, VIRT);
        }

        /* Create a new PD, set up the next PDPT entry. */
      	vaddr = vkmalloc(vk_heap, 1);
        vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
        pd = (struct page_directory *)vaddr;
        kmemset(pd, 0, PAGE_SIZE);

        entry = &pdpt->ept_entries[pdpt_idx];
        init_entry(entry, pd, VIRT);
      }
      /* Create a new PT, set up the next PD entry. */
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
      pt = (struct page_table *)vaddr;
      kmemset(pt, 0, PAGE_SIZE);

      entry = &pd->ept_entries[pd_idx];
      init_entry(entry, pt, VIRT);
    }

    page = &pt->ept_entries[pt_idx];
    paddr = get_free_frame();
    init_ept_page(page, paddr, PHYS, CACHE);

    /* Create a temporary mapping if we reach the memory map */
    if(pml4t_idx == virt2pml4t(MEMORY_MAP) &&
       pdpt_idx  == virt2pdpt(MEMORY_MAP)  && 
       pd_idx    == virt2pd(MEMORY_MAP)    &&
       pt_idx    == virt2pt(MEMORY_MAP)) {
      vaddr = vkmalloc(vk_heap, 1);
      attach_page(vaddr, paddr, PG_RW);
      vp->memmap = (struct memmap *)(vaddr + (MEMORY_MAP % PAGE_SIZE));
      vp->memmap_entries = 
	(uint16_t*)(vaddr + (MEMORY_MAP_ENTRIES % PAGE_SIZE));
      *(vp->memmap_entries) = 0x0;
    }
      
    pt_idx++;
  }

  /* this giant chunk will be added to the memmap as two regions:
     1: unusable low memory where "boot1" (in our case the hypv pretending
     to be boot1) put init page tables and other stuff
     2: Actual useable memory
  */
  add_to_memmap(vp, 0, 0x20000, 6);
  add_to_memmap(vp, 0x20000, PHYS_VGA_MEM - 0x20000, 1);

  /* vga not usable for guest */
  add_to_memmap(vp, PHYS_VGA_MEM, 0x8000,3);

  add_to_memmap(vp, PHYS_VGA_MEM+0x8000, vp->memsz - PHYS_VGA_MEM+0x8000,1);

  /* Now make the ramdisk visible and known to the EPT system
   * That way, the kernels RamIO requests will be correctly translated
   * into physical addresses.
   */
  /* Set up initial values for page table structure. */

  for(i = 0; i < RAMDISK_SIZE; i += PAGE_SIZE) {

    if(pt_idx == 512) {
      pt_idx = 0;
      pd_idx++;
    }
    if(pd_idx == 512) {
      pd_idx = 0;
      pdpt_idx++;
    }
    if(pdpt_idx == 512) {
      pdpt_idx = 0;
      pml4t_idx++;
    }
    entry = &pml4t->ept_entries[pml4t_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, PG_RW);
      pdpt = (struct page_directory_pointer_table *)vaddr;
      kmemset(pdpt, 0, PAGE_SIZE);

      init_entry(entry, pdpt, VIRT);
    } else {
      pdpt = phys2virt(TABLE2ADDR(entry->addr));
    }
    entry = &pdpt->ept_entries[pdpt_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, PG_RW);
      pd = (struct page_directory *)vaddr;
      kmemset(pd, 0, PAGE_SIZE);

      init_entry(entry, pd, VIRT);
    } else {
      pd = phys2virt(TABLE2ADDR(entry->addr));
    }
    entry = &pd->ept_entries[pd_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, PG_RW);
      pt = (struct page_table *)vaddr;
      kmemset(pt, 0, PAGE_SIZE);

      init_entry(entry, pt, VIRT);
    } else {
      pt = phys2virt(TABLE2ADDR(entry->addr));
    }
    page = &pt->ept_entries[pt_idx];

    init_ept_page(page, *(uint64_t *)RAMDISK + i, PHYS, CACHE);

    pt_idx++;
  }

  /* previously we added to the memmap vp->memsize worth of chunks. the
     ramdisk was appended to that, so we start at vp->memsize as a guest
     physical address. */
  add_to_memmap(vp, vp->memsz, RAMDISK_SIZE, 2);

  /* Create the mappings for the ACPI table. */
  if(rsdpdesc.Revision == 0) 
    addr = rsdpdesc.RsdtAddress;
  else 
    addr = rsdpdesc.XsdtAddress;
  
  if(rsdpdesc.Length == 0) 
    length = PAGE_SIZE;
  else 
    length = rsdpdesc.Length;
  
  /* Page align. */
  if(addr % PAGE_SIZE) {
    length += addr % PAGE_SIZE;
    addr += (addr % PAGE_SIZE) - PAGE_SIZE;
  }

  /* calculate the start and end of the acpi tables in real ram */
#ifdef DEBUG_ACPI
  kprintf("RsdtAddress = 0x%x, acpi_start = 0x%x\n", rsdpdesc.RsdtAddress,
          acpi_start);
#endif
  for(i = 0; i < *numchunks; i++, chunk++ ) {

    if ( chunk->length == 0 )
      continue;
    if ( chunk->type == 3 )
      break;
  }

  acpi_start = chunk->base;
  acpi_end = acpi_start + chunk->length;

  entry_ptr = (uint32_t*)((struct ACPIHeader *)(uintptr_t)rsdpdesc.RsdtAddress+1);
  end_rsdt = (uint32_t*)(((uint8_t*)(uintptr_t)rsdpdesc.RsdtAddress)+((struct ACPIHeader *)(uintptr_t)rsdpdesc.RsdtAddress)->Length)-1;
  if(first) {
    while ( entry_ptr <= end_rsdt ) {
      *entry_ptr = (vp->memsz+RAMDISK_SIZE) + (*entry_ptr - acpi_start);
      entry_ptr++;
    }
    first = 0;
  }

  // uber code left here for lolz.
  //  *(uint32_t*)((struct ACPIHeader *)(uintptr_t)rsdpdesc.RsdtAddress+1) = (vp->memsz+RAMDISK_SIZE) + (*(uint32_t*)((struct ACPIHeader *)(uintptr_t)rsdpdesc.RsdtAddress+1)-acpi_start);
  //    *((uint32_t*)(((uint8_t*)(uintptr_t)rsdpdesc.RsdtAddress)+((struct ACPIHeader *)(uintptr_t)rsdpdesc.RsdtAddress)->Length)-1) = (vp->memsz + RAMDISK_SIZE) + (*((uint32_t*)(((uint8_t*)(uintptr_t)rsdpdesc.RsdtAddress)+((struct ACPIH eader *)(uintptr_t)rsdpdesc.RsdtAddress)->Length)-1) - acpi_start);

  /* BIOS location of the RSDP descriptor */
  /* copy the RSDPDescriptor into low memmory of the first chunk we created.
     Then update the address of the ACPI tables that it points to, to now
     point to the new block we will add to the ept, which contains the mappings
     to the ACPI tables. */
  vaddr = vkmalloc(vk_heap, 1);
  paddr = ept_walk(0x9fc00, (uint64_t)virt2phys(pml4t), 0, 0, 0);

  attach_page(vaddr, paddr, PG_RW);

  kmemcpy((void*)(vaddr + (0x9fc00 % PAGE_SIZE)), &rsdpdesc, 
	  sizeof(struct RSDPDescriptor20));

  guest_rsdp = (struct RSDPDescriptor20*)(vaddr + (0x9fc00 % PAGE_SIZE));

  guest_rsdp->RsdtAddress = (vp->memsz+RAMDISK_SIZE) + 
    (rsdpdesc.RsdtAddress - acpi_start);

  /* remove the temporary mapping */
  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE);
  vkdirty(vk_heap, (void*)vaddr, 1);

  *(uint32_t*)(uintptr_t)rsdpdesc.RsdtAddress = (vp->memsz+RAMDISK_SIZE) + 
    (*(uint32_t*)(uintptr_t)rsdpdesc.RsdtAddress - acpi_start);

  /* adding two extra pages to the end of the acpi tables allows for end
     cases where the table starts on one boundary, but data ends on another */
  acpi_end += 2*PAGE_SIZE;

  acpi_end += PAGE_SIZE - (acpi_end % PAGE_SIZE);

  /* tack it on the end as we did with the ramdisk */
  for(j = acpi_start; j < acpi_end; j += PAGE_SIZE) {

    if(pt_idx == 512) {
      pt_idx = 0;
      pd_idx++;
    }
    if(pd_idx == 512) {
      pd_idx = 0;
      pdpt_idx++;
    }
    if(pdpt_idx == 512) {
      pdpt_idx = 0;
      pml4t_idx++;
    }

    entry = &pml4t->ept_entries[pml4t_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
      pdpt = (struct page_directory_pointer_table *)vaddr;
      kmemset(pdpt, 0, PAGE_SIZE);

      init_entry(entry, pdpt, VIRT);
    } else {
      pdpt = phys2virt(TABLE2ADDR(entry->addr));
    }
    entry = &pdpt->ept_entries[pdpt_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
      pd = (struct page_directory *)vaddr;
      kmemset(pd, 0, PAGE_SIZE);

      init_entry(entry, pd, VIRT);
    } else {
      pd = phys2virt(TABLE2ADDR(entry->addr));
    }
    entry = &pd->ept_entries[pd_idx];
    if(!(EPT_PRESENT(entry->bits))) {
      vaddr = vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)vaddr,PAGE_SIZE, 0);
      pt = (struct page_table *)vaddr;
      kmemset(pt, 0, PAGE_SIZE);

      init_entry(entry, pt, VIRT);
    } else {
      pt = phys2virt(TABLE2ADDR(entry->addr));
    }
    page = &pt->ept_entries[pt_idx];
    init_ept_page(page, j, PHYS, CACHE);
    pt_idx++;
  }

  /* we need to add this to the memmap */
  add_to_memmap(vp, vp->memsz+RAMDISK_SIZE, acpi_end - acpi_start, 3);

  /* Set the extended page table pointer. */
  vp->peptp = virt2phys(pml4t);             /* The PML4T */
  vp->peptp |= (3 << 3);                    /* Page-walk length */
  vp->peptp |= 6;                           /* Make EPT structures cacheable*/
  
#ifdef DEBUG
  kprintf("Finished creating EPT structures vp->ept 0x%x\n",vp->peptp);
#endif

  /* Clean up the temporary mappings */
  vmem_free_temp((uint64_t*)((uint64_t)vp->memmap & ~0xFFF), PAGE_SIZE);
  vkdirty(vk_heap, (uint64_t*)((uint64_t)vp->memmap & ~0xFFF), 1);
  vp->memmap = 0x0;
  vp->memmap_entries = 0x0;

  vmwrite(EPT_POINTER, vp->peptp);
  vmwrite(EPT_POINTER_HIGH, (vp->peptp >> 32));

  return;
} 

uint64_t ept_walk(uint64_t guest_paddr,
		  uint64_t peptp,
		  struct page_directory_pointer_table **pdpt,
		  struct page_directory **pd,
		  struct page_table **pt) {
		  
  struct page_map_level_4_table *pml4t = 
    (struct page_map_level_4_table *)phys2virt(peptp);
  int pml4t_idx = virt2pml4t(guest_paddr);
  int pdpt_idx = virt2pdpt(guest_paddr);
  int pd_idx = virt2pd(guest_paddr);
  int pt_idx = virt2pt(guest_paddr);
  struct page_directory_pointer_table *_pdpt;
  struct page_directory *_pd;
  struct page_table *_pt;
  
  if ( EPT_PRESENT(pml4t->ept_entries[pml4t_idx].bits) ) {
    _pdpt = (struct page_directory_pointer_table *)
      phys2virt(TABLE2ADDR(pml4t->ept_entries[pml4t_idx].addr));
    if(pdpt != NULL)
      *pdpt = _pdpt;

    if ( EPT_PRESENT(_pdpt->ept_entries[pdpt_idx].bits) ) {

      _pd = (struct page_directory *)
	phys2virt(TABLE2ADDR(_pdpt->ept_entries[pdpt_idx].addr));
      if(pd != NULL)
	*pd = _pd;

      if ( _pd->ept_entries[pd_idx].bits ) {

	_pt = (struct page_table *)
	  phys2virt(TABLE2ADDR(_pd->ept_entries[pd_idx].addr));
	if(pt != NULL)
	  *pt = _pt;
	
	return TABLE2ADDR(_pt->ept_entries[pt_idx].addr);
      }
      else { 
	if ( pt )
	  *pt = NULL;
      }
    } 
    else {
      if ( pd )
	*pd = NULL;
      if ( pt )
	*pt = NULL;
    }
  }
  else {
    if ( pd )
      *pd = NULL;
    if ( pt )
      *pt = NULL;
    if ( pdpt )
      *pdpt = NULL;
  }

  return 0x0;
}

void ept_map_page(uint64_t peptp, uint64_t gpaddr,
		  uint64_t paddr, uint8_t cache_type) {
  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;

  pml4t = (struct page_map_level_4_table *)phys2virt(peptp);

  /* Find or create the pdpt */
 if( ept_walk(gpaddr, peptp, &pdpt, &pd, &pt) != MEM_FAIL) {
    if ( !pt ) { 
      if ( !pd ) {
	if ( !pdpt ) {
	  pdpt = (struct page_directory_pointer_table *)vkmalloc(vk_heap, 1);
	  vmem_alloc((uint64_t*)pdpt, PAGE_SIZE, PG_RW);
	  
	  kmemset(pdpt, 0, PAGE_SIZE);
	  init_entry(&pml4t->ept_entries[virt2pml4t(gpaddr)], pdpt, VIRT);
	}
	pd = (struct page_directory *)vkmalloc(vk_heap, 1);
	vmem_alloc((uint64_t*)pd, PAGE_SIZE, PG_RW);
	
	kmemset(pd, 0, PAGE_SIZE);
	init_entry(&pdpt->ept_entries[virt2pdpt(gpaddr)], pd, VIRT);
      }
      pt = (struct page_table *)vkmalloc(vk_heap, 1);
      vmem_alloc((uint64_t*)pt, PAGE_SIZE, PG_RW);
      
      kmemset(pt, 0, PAGE_SIZE);
      init_entry(&pd->ept_entries[virt2pd(gpaddr)], pt, VIRT);
    }


    /*ept_map_page is a destructive function in terms of ept page entries    */
    /*i.e. it will overwrite any entry that is already contained in the ept. */
    /*So, we must check it is a frame that can be freed physically through   */
    /*the tf bit (not ramdisk or something like that ) and we must check to  */
    /*see if the frame was ever mapped into the ept                          */
    if ( EPT_PRESENT( pt->ept_entries[virt2pt(gpaddr)].bits) ) {

	framearray[TABLE2ADDR(pt->ept_entries[virt2pt(gpaddr)].addr)
		   /PAGE_SIZE].vaddr = 0x0;
	framearray[TABLE2ADDR(pt->ept_entries[virt2pt(gpaddr)].addr)
		   /PAGE_SIZE].free  = 0x1;
	framearray[TABLE2ADDR(pt->ept_entries[virt2pt(gpaddr)].addr)
		   /PAGE_SIZE].proc  = 0x0;
    }

    init_ept_page(&pt->ept_entries[virt2pt(gpaddr)], paddr, PHYS, 
		  cache_type == EPT_TYPE_UNCACHEABLE ? UNCACHE : CACHE);
 }
  
  return;
}

void ept_free_pml4t(struct page_map_level_4_table *pml4t) {
  int i;
  union ept_pt_entry *pml4te;

  for(i=0; i<512; i++) {
    pml4te = &(pml4t->ept_entries[i]);
    if (EPT_PRESENT(pml4te->bits)) {
#ifdef DEBUG
      kprintf("Freeing eptpml4t entry %d...\n", i);
#endif
      ept_free_pdpt(phys2virt(TABLE2ADDR(pml4te->addr)));
    }
  }
  vmem_free( (uint64_t *)pml4t, PAGE_SIZE );
  vkdirty( vk_heap, (uint64_t*)pml4t, 1 );
  vkfreedirty( vk_heap );
  flush_tlb( 0 );
  __invept( 2, (uint64_t)pml4t );
  asm volatile( "wbinvd" );
  return;
}

void ept_free_pdpt(struct page_directory_pointer_table *pdpt) {
  int i;
  union ept_pt_entry *pdpte;

  for(i=0; i<512; i++) {
    pdpte = &(pdpt->ept_entries[i]);
    if (EPT_PRESENT(pdpte->bits))
      ept_free_pd(phys2virt(TABLE2ADDR(pdpte->addr)));
  }
  vmem_free((uint64_t *)pdpt, PAGE_SIZE);
  vkdirty(vk_heap, (uint64_t*)pdpt, 1);
  return;
}


void ept_free_pd(struct page_directory *pd) {
  int i;
  union ept_pt_entry *pde;

  for(i=0; i<512; i++) {
    pde = &(pd->ept_entries[i]);
    if (EPT_PRESENT(pde->bits))
      ept_free_pt(phys2virt(TABLE2ADDR(pde->addr)));
  }
  vmem_free((uint64_t *)pd, PAGE_SIZE);
  vkdirty(vk_heap, (uint64_t*)pd, 1);
  return;
}

void ept_free_pt(struct page_table *pt) {
  int i;
  union ept_page *page;

  for(i=0; i<512; i++) {
    page = &(pt->ept_entries[i]);
    if (EPT_PRESENT(page->bits)){
      framearray[TABLE2ADDR(page->addr)/PAGE_SIZE].vaddr = 0x0;
      framearray[TABLE2ADDR(page->addr)/PAGE_SIZE].free  = 0x1;
      framearray[TABLE2ADDR(page->addr)/PAGE_SIZE].proc  = 0x0;
    }
  }

  vmem_free((uint64_t *)pt, PAGE_SIZE);
  vkdirty(vk_heap, (uint64_t*)pt, 1);
  return;
}


/*
 * Roughly the same as above but wipes TLB trnaslations based on the
 * eptp pointer, note here however there are only two options
 * 1 single context only the current EPT everythiing associated with bits 51:12
 * 2 global context all EPT's
 */
/* without the static declaration in front of inline the compiler ignores it
 * suggest removal of inline if this function is used in other file (RMD)*/
inline int __invept(uint64_t type, uint64_t eptp){
  struct {
    uint64_t reserved : 64;
    uint64_t eptp;
  } operand = {eptp};
  uint8_t invalid, valid;

  kprintf("type type\n");
  if(type != 1 && type !=2)
    return 1;

  /*
   * FIXME need to check CR4 to see if the proc supports both single and all 
   * context invalidation                                                         */
  asm volatile ("invept %2, %3\n\t"
		VM_STATUS_CHECK
		: VM_STATUS_CONSTRAINTS(invalid, valid) 
		: "m" (operand), "r"(type) : "memory" );

  kprintf("invalid %d valid %d\n", invalid, valid);

  return 0;
}

void destroy_ept(vproc_t *vp) {
  struct page_map_level_4_table *pml4t;

  /* phys addr of pml4t is eptp with the first 12 bits masked */
  pml4t = phys2virt((vp->peptp) & (~((uint64_t)(0xFFF))));

  vmem_free((uint64_t*)vp->virt_apic_access_addr, PAGE_SIZE);
  vkdirty(vk_heap, (uint64_t*)vp->virt_apic_access_addr, 1);
  vkfreedirty(vk_heap);
  flush_tlb(0);

  ept_free_pml4t(pml4t);
}

#ifdef XONLY_BEAR_HYPV
void mark_kernel_execute_only(vproc_t *vp) {
  uint64_t pml4t_phys, pml4t_virt;
  uint64_t pdpt_phys, pdpt_virt;
  uint64_t pd_phys, pd_virt;
  uint64_t pt_phys, pt_virt;
  uint64_t guest_frame_phys;
  struct page_table *ept_pt;

  int pml4t_idx, pdpt_idx, pd_idx, pt_idx;

  pml4t_virt = vkmalloc(vk_heap, 1);
  pdpt_virt = vkmalloc(vk_heap, 1);
  pd_virt = vkmalloc(vk_heap, 1);
  pt_virt = vkmalloc(vk_heap, 1);

  /* attach guest PML4T */
  pml4t_phys = ept_walk(vp->vmcs.GUEST_CR3, vp->peptp, 0, 0, 0);
  attach_page(pml4t_virt, pml4t_phys, PG_RW);

  for ( pml4t_idx = 0; pml4t_idx < 512; pml4t_idx++ ) {

    if(!((struct page_table*)pml4t_virt)->entries[pml4t_idx].present)
      continue;    

    /* attach next guest PDPT */
    pdpt_phys = ept_walk(
			 TABLE2ADDR(((struct page_table*)pml4t_virt)->entries[pml4t_idx].addr),
			 vp->peptp, 0, 0, 0);
    attach_page(pdpt_virt, pdpt_phys, PG_RW);
    flush_tlb(0);

    /* look through all pdpt entries, or skip loop if not present. */
    for ( pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++ ) {

      if(!((struct page_table*)pdpt_virt)->entries[pdpt_idx].present)
	continue;
      
      /* attach next guest PD */
      pd_phys = ept_walk(
			 TABLE2ADDR(((struct page_table*)pdpt_virt)->entries[pdpt_idx].addr),
			 vp->peptp,0,0,0);
      
      attach_page(pd_virt, pd_phys, PG_RW);
      flush_tlb(0);
  
      /* look through all pd entries, or skip loop if not present */
      for ( pd_idx = 0; pd_idx < 512; pd_idx++ ) {

	if( !((struct page_table*)pd_virt)->entries[pd_idx].present)
	  continue;

	/* attach next guest PT */
	pt_phys = ept_walk(
			   TABLE2ADDR(((struct page_table*)pd_virt)->entries[pd_idx].addr),
			   vp->peptp, 0, 0, 0);

	attach_page(pt_virt, pt_phys, PG_RW);
	flush_tlb(0);

	/* look through all pt entries, or skip loop if not present */
	for ( pt_idx = 0; pt_idx < 512; pt_idx++ ) {

	  if (  ((struct page_table*)pt_virt)->entries[pt_idx].nx    ||
		((struct page_table*)pt_virt)->entries[pt_idx].rw    ||
		!((struct page_table*)pt_virt)->entries[pt_idx].global || 
		!((struct page_table*)pt_virt)->entries[pt_idx].present ) {
	    continue;
	  }

	  /* physical address of a kernel code page */
	  guest_frame_phys = TABLE2ADDR(((struct page_table*)pt_virt)->entries[pt_idx].addr);

	  /* find the virtual address for the appropriate EPT PT */
	  ept_walk(guest_frame_phys, vp->peptp, 0, 0, &ept_pt);

	  /* Mark the frame as execute only */
	  ept_pt->ept_entries[virt2pt(guest_frame_phys)].w = 0;
	  ept_pt->ept_entries[virt2pt(guest_frame_phys)].r = 0;
	}

	vmem_free_temp((uint64_t*)pt_virt, PAGE_SIZE);
      }

      vmem_free_temp((uint64_t*)pd_virt, PAGE_SIZE);
    }

    vmem_free_temp((uint64_t*)pdpt_virt, PAGE_SIZE);
  }

  vmem_free_temp((uint64_t*)pml4t_virt, PAGE_SIZE);
  vkdirty(vk_heap, (void*)pml4t_virt, 1);
  vkdirty(vk_heap, (void*)pdpt_virt, 1);
  vkdirty(vk_heap, (void*)pd_virt, 1);
  vkdirty(vk_heap, (void*)pt_virt, 1);

  return;
}
#endif
