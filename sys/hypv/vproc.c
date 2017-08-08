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
 * vproc.c 
 * functions creating, destroying, and otherwise modifying vprocs and
 * their memory spaces.
 */
#include <asm_subroutines.h>
#include <constants.h>
#include <disklabel.h>
#include <diversity.h>
#include <elf.h>
#include <elf_loader.h>
#include <file_abstraction.h>
#include <khash.h>
#include <vmx.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <interrupts.h>
#include <memory.h>
#include <pci.h>
#include <pes.h>
#include <kqueue.h>
#include <ff_const.h>
#include <ramio.h>
#include <memory.h>
#include <acpi.h>
#include <fatfs.h>
#include <ept.h>
#include <vproc.h>
#include <ept.h>
#include <kvmem.h>
#include <vmexit.h>
#include <apic.h>
#include <vmx_utils.h>
#include <smp.h>
#include <../hypv/asm.h>

hashtable_t *vprocs;		/* hash table of vprocs               */

/*starting off at 1 because 0 is reserved in special cases
 * anything unassigned for VTD ends up in domain 0, because Intel trageted the 
 * linux enviornment when they created the silicon, this can be very bad if 
 * you're unaware of it, esp when we go to flush the cache later. 
*/ 
int vpid_counter = 1;			/* Virtual Process Identifier      */

int get_vpid_cnt()
{
 return vpid_counter;

}

/* Internal helper functions. */
static void copy_bootstuff(vproc_t *);
static void create_guest_pagetables(uint64_t);

vproc_t *create_vproc(char *file) {
  vproc_t *vp;
  struct elf_ctx *file_ctx;
  struct bad_region br[3];
  uint64_t i, vaddr, paddr;
  int status; 
  FILINFO stat;
  uint64_t entry_point;

  vp = kmalloc_track(VPROC_SITE,sizeof(vproc_t));
  kmemset(vp, 0, sizeof(vproc_t));
  vp->running = 0;
	
  vp->memsz = GUEST_MEM_SIZE;
	
  br[0].start = MEMORY_MAP_ENTRIES;
  br[0].end = br[0].start + sizeof(uint16_t) + sizeof(struct memmap);

  create_ept(vp);
  
  /* Copy the basic segmentation info. */
  copy_bootstuff(vp);
  
  /* Develop the (guest) page tables. */
  create_guest_pagetables(vp->peptp);

  /* Reposition the duals for what should correspond to the console (video)
   * memory in the guest -- in terms of guest-physical or hypervisor-linear,
   * this is (vp->memory + PHYS_VGA_MEM) to (vp->memory + PHYS_VGA_MEM +
   * 0x8000).  */
  for(i = PHYS_VGA_MEM; i < PHYS_VGA_MEM + 0x8000; i += PAGE_SIZE) {
      ept_map_page(vp->peptp, i, i, EPT_TYPE_UNCACHEABLE);      
  }
  
  /* 
   * Reposition the duals for what should correspond to the NIC cards' MMIO
   * registers in the guest -- these memory ranges are defined by the PCI
   * BAR registers.
   */
#ifndef STANDALONE
  pci_apply2(vp, &hypv_map_bce_mmio);
#endif

  /* Load the file. */
  file_ctx = alloc_elf_ctx(file_read, file_seek, file_error_check);
  f_stat(file, &stat);
  file_ctx->file_ctx = file_open(file);
  vp->file = file_ctx;

  /* As defined in boot1.S - DISKLABEL + SIZE OF BOOT1 - MEM_BASE */
  entry_point = *(uint64_t*)0x7e28 + 0x7e00 - 0x200;

  vaddr = vkmalloc(vk_heap, (stat.fsize / PAGE_SIZE) + 1);
  for(i = 0; i < ((stat.fsize / PAGE_SIZE) + 1); i++) {
    paddr = ept_walk(entry_point + (i*PAGE_SIZE), vp->peptp, 0, 0, 0);
    attach_page(vaddr + i*PAGE_SIZE, paddr, PG_RW);
  }

  file_read(file_ctx->file_ctx, 
	    (void*)(vaddr + (entry_point % PAGE_SIZE)), 
	    stat.fsize);
#ifdef DEBUG 
  kprintf("kboot2 = %x\n", *(uint64_t*)(vaddr + (entry_point % PAGE_SIZE)));
#endif
  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE*((stat.fsize / PAGE_SIZE) + 1));
  vkdirty(vk_heap, (void*)vaddr, (stat.fsize / PAGE_SIZE) + 1);

  file_close(file_ctx->file_ctx);

  vaddr = vkmalloc(vk_heap, 1);
  vmem_alloc((uint64_t*)vaddr, PAGE_SIZE, PG_RW);
  
  vp->vmcs_ptr = (void*)vaddr; 
  vp->vmcs_ptr_phys = virt2phys(vp->vmcs_ptr);
  kmemset(vp->vmcs_ptr, 0, PAGE_SIZE);

  /* Initialize the vproc_t copy of the VMCS with the defaults. */
  kmemcpy(&(vp->vmcs), &vmcs_default, sizeof(struct vmcs));
  vp->vproc_id = vpid_counter;
  vp->vmcs.VIRTUAL_PROCESSOR_ID = vpid_counter++;
#ifdef DEBUG
  kprintf("vmcs vpid is %d \n",   vp->vmcs.VIRTUAL_PROCESSOR_ID);
#endif
  vp->vmcs.GUEST_RIP = entry_point;
  vp->vmcs.GUEST_RSP = 0x7BF8;
  vp->vmcs.GUEST_CR3 = INIT_PAGE_PML4T;
  vp->vmcs.EPT_POINTER = vp->peptp;
  vp->vmcs.EPT_POINTER_HIGH = vp->peptp >> 32;

  vector_init(&vp->pending_interrupts_vec, 
	      sizeof(waiting_interrupt_t), 5, NULL);
  
  /* The following lines create the APIC ACCESS PAGE, which controls how  */
  /* the hypervisor controls accesses to the APIC. For this to work the   */
  /* ept must already exist. The guest will believe that it is acesses    */
  /* the apic mmio page, which is typically 0xfee00000. Instead the ept   */
  /* redirects it to the physical frame of the APIC ACCESS PAGE. The vmcs */
  /* structure contains this physical frames addres, which supersedes the */
  /* ept translation and causes a vmexit that the hypervisor handles.     */

  /* Further and seperate note: nothing is ever written to this page/frame.*/
  /* Thus, multiple cores should be able to share it. Will need to test in */
  /* the near future. (RMD)                                                */
  vp->virt_apic_access_addr = vkmalloc(vk_heap, 1);
  vmem_alloc((uint64_t*)vp->virt_apic_access_addr, PAGE_SIZE, PG_RW);
  
  vp->vmcs.APIC_ACCESS_ADDR = virt2phys((void*)vp->virt_apic_access_addr);
  vp->vmcs.APIC_ACCESS_ADDR_HIGH = (vp->vmcs.APIC_ACCESS_ADDR) >> 32;

#ifdef DEBUG
  kprintf("ACCESS ADDR = 0x%x\n", vp->vmcs.APIC_ACCESS_ADDR);
#endif

  ept_map_page(vp->peptp, 0xFEE00000, vp->vmcs.APIC_ACCESS_ADDR, EPT_TYPE_UNCACHEABLE);

  /* IOAPIC still must be mapped through */
  ept_map_page(vp->peptp, 0xFEC00000, 0xFEC00000, EPT_TYPE_UNCACHEABLE);
 
  /* Insert the revision ID before anything else. The VMX instructions
   * require it. */
  revid(vmx_msrs.MSR_IA32_VMX_BASIC, vp->vmcs_ptr);

  /* Clear and load. */
  if((status = vmclear(vp->vmcs_ptr_phys))) {
      kputs("[Hypv] error setting up vmcs vmclear\n");
      panic();
  }	

  if((status = vmptrld(vp->vmcs_ptr_phys))) {
    kprintf("vmprtld status = 0%d\n", status);
    kputs("[Hypv] error setting up vmcs vmptrld\n");
    panic();
  }
		
  write_vmcs(&vp->vmcs);

  /* The SSE part of the general-purpose registers. */
  vp->reg_storage.sse = pes_new_save();
  /* Modification queue for VMCS. */
  vp->vmcs_mods = qopen();

  load_vproc(vp);

  /* Add it to the vproc table. */
  vproc_put_all(vp);

  return vp;
}

vproc_t *join_to_vproc(vproc_t *vp_to_join) {
  vproc_t *vp;
  uint64_t vaddr;
  uint64_t entry_point;
  struct gdt_desc desc;

  vp = kmalloc_track(VPROC_SITE,sizeof(vproc_t));
  kmemset(vp, 0, sizeof(vproc_t));
  /*set the initial status */
  vp->running = 0;
  vp->active = 0;
  vp->launched = 0;
  vp->peptp = vp_to_join->peptp;

  /* As defined in boot1.S - DISKLABEL + SIZE OF BOOT1 - MEM_BASE */
  entry_point = *(uint64_t*)0x7e28 + 0x7e00 - 0x200;

  /*create the VMCS */
  vaddr = vkmalloc(vk_heap, 1);
  vmem_alloc((uint64_t*)vaddr, PAGE_SIZE, PG_RW);
  
  vp->vmcs_ptr = (void*)vaddr; 
  vp->vmcs_ptr_phys = virt2phys(vp->vmcs_ptr);
  kmemset(vp->vmcs_ptr, 0, PAGE_SIZE);

  /* Initialize the vproc_t copy of the VMCS with the defaults. */
  kmemcpy(&(vp->vmcs), &vmcs_default, sizeof(struct vmcs));

  vp->vmcs.VIRTUAL_PROCESSOR_ID = vp_to_join->vmcs.VIRTUAL_PROCESSOR_ID;

  vp->vproc_id = vp_to_join->vmcs.VIRTUAL_PROCESSOR_ID;
#ifdef DEBUG
  kprintf("[HYPV] vmcs vpid is %d \n",   vp->vmcs.VIRTUAL_PROCESSOR_ID);
#endif
  vp->vmcs.GUEST_RIP = entry_point;
  vp->vmcs.GUEST_RSP = 0x7BF8;
  vp->vmcs.GUEST_CR3 = INIT_PAGE_PML4T;
  vp->vmcs.EPT_POINTER = vp_to_join->vmcs.EPT_POINTER;
  vp->vmcs.EPT_POINTER_HIGH = vp_to_join->vmcs.EPT_POINTER_HIGH;

  vector_init(&vp->pending_interrupts_vec, 
	      sizeof(waiting_interrupt_t), 5, NULL);
	
  vp->vmcs.APIC_ACCESS_ADDR = virt2phys((void*)vp_to_join->virt_apic_access_addr);
  vp->vmcs.APIC_ACCESS_ADDR_HIGH = (vp_to_join->vmcs.APIC_ACCESS_ADDR) >> 32;

  asm volatile("sgdt %0" : "=m"(desc));
  vmcs_default.HOST_GDTR_BASE = desc.base;
  vmcs_default.GUEST_GDTR_BASE = vp_to_join->vmcs.HOST_GDTR_BASE;
  vmcs_default.GUEST_GDTR_LIMIT = vp_to_join->vmcs.HOST_GDTR_BASE;

  asm volatile("sidt %0" : "=m"(desc));
  vmcs_default.HOST_IDTR_BASE = desc.base;
  vmcs_default.GUEST_IDTR_BASE = vp_to_join->vmcs.GUEST_IDTR_BASE;
  vmcs_default.GUEST_IDTR_LIMIT = vp_to_join->vmcs.GUEST_IDTR_LIMIT;


  /* Insert the revision ID before anything else. The VMX instructions
   * require it. */
  revid(vmx_msrs.MSR_IA32_VMX_BASIC, vp->vmcs_ptr);

  /* The SSE part of the general-purpose registers. */
  vp->reg_storage.sse = pes_new_save();
  /* Modification queue for VMCS. */
  vp->vmcs_mods = qopen();

  /* Add it to the vproc table. */
  vproc_put_all(vp);

  return vp;
}

void destroy_vproc(vproc_t *vp) {
  vproc_remove(vprocs_all, vp->vproc_id);	/* Take out of vproc table  */
  free_elf_ctx(vp->file);	       /* Free the elf context info*/
  pes_free_save(vp->reg_storage.sse);/* Free extended state save */
  vmem_free(vp->vmcs_ptr, PAGE_SIZE);  /* Free vmcs region         */
  vkdirty(vk_heap, vp->vmcs_ptr, 1);
  destroy_ept(vp);                     /* Free ept structures      */
  vmem_free( (uint64_t*)vp->virt_apic_access_addr, PAGE_SIZE);
  vkdirty( vk_heap, (uint64_t*)vp->virt_apic_access_addr, 1);
  vkfreedirty( vk_heap );
  flush_tlb( 0 );
  qclose(vp->vmcs_mods);               /* Close modification queue */
  vector_dispose(&vp->pending_interrupts_vec);
  kfree_track(VPROC_SITE,vp);          /* Free vproc struct        */

  return;
}

static void copy_bootstuff(vproc_t *vp) {
  struct disklabel *label;
  struct gdt_desc gdt;
  uint64_t vaddr, paddr;

  /* The GDT. */
  asm("sgdt %0" : "=m"(gdt));

  vaddr = vkmalloc(vk_heap,1);
  paddr = ept_walk(gdt.base, vp->peptp, NULL, NULL, NULL);
  attach_page(vaddr, paddr, PG_RW);

  kmemcpy((void*)(vaddr + (gdt.base % PAGE_SIZE)), 
	  (void *)(gdt.base), 
	  0x37);

  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE);

  vkdirty(vk_heap, (void*)vaddr, 1);

  vaddr = vkmalloc(vk_heap, 1);
  paddr = ept_walk(SLICE_OFFSET, vp->peptp, NULL, NULL, NULL);
  attach_page(vaddr, paddr, PG_RW);  

  /* Information for filesystem setup. */
  kmemcpy((void*)(vaddr + (SLICE_OFFSET % PAGE_SIZE)),
	  (void *)SLICE_OFFSET,
	  sizeof(uint16_t));

  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE);
  vkdirty(vk_heap, (void*)vaddr, 1);

  /* The MBR region, which was overwritten with the disklabel. */
  label = (struct disklabel *)SLICE_MBR_START;
#ifdef DEBUG
  kprintf("	[vproc]MBR Region label = %d\n", label->total_size);
#endif

  vaddr = vkmalloc(vk_heap, 1);
  paddr = ept_walk(SLICE_MBR_START, vp->peptp, NULL, NULL, NULL);
  attach_page(vaddr, paddr, PG_RW);  

  kmemcpy((void*)(vaddr + (SLICE_MBR_START % PAGE_SIZE)),
	  (void *)SLICE_MBR_START,
	  label->total_size);

  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE);
  vkdirty(vk_heap, (void*)vaddr, 1);

  /*TODO frames need to be freed later */
  return;
}

/* Create the starting page-tables in the guest. These mirror the page-tables
 * created for the hypervisor by boot1 (and the kernel by the
 * bootloader, if you aren't using the hypervisor).
 */
static void create_guest_pagetables(uint64_t peptp) {
  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;
  union pt_entry *entry;
  union page *page;
  int i;
  uint64_t paddr;

  /* Just like the bootloader, set 0x1000 to be the PML4T, 0x2000 the PDPT,
   * 0x3000 the PDT, and 0x4000 the PT. */
  pml4t = (struct page_map_level_4_table *)vkmalloc(vk_heap, 4);
  paddr = ept_walk(INIT_PAGE_PML4T, peptp, 0, 0, 0);
  attach_page((uint64_t)pml4t, paddr, PG_RW);
  kmemset(pml4t, 0, PAGE_SIZE);
  entry = &(pml4t->entries[0]);
  entry->present = 1;
  entry->rw = 1;
  entry->nx = 0;
  entry->addr = ADDR2TABLE(INIT_PAGE_PDPT);

  /* create the recursive mapping for the guest */
  entry = &(pml4t->entries[PML4T_RECURSIVE_ENTRY_IDX]);
  entry->present = 1;
  entry->rw = 1;
  entry->nx = 0;
  entry->addr = ADDR2TABLE(INIT_PAGE_PML4T);	

  pdpt = (struct page_directory_pointer_table*)((uint64_t)pml4t + PAGE_SIZE);
  paddr = ept_walk(INIT_PAGE_PDPT, peptp, 0, 0, 0);
  attach_page((uint64_t)pdpt, paddr, PG_RW);
  kmemset(pdpt, 0, PAGE_SIZE);
  entry = &(pdpt->entries[0]);
  entry->present = 1;
  entry->rw = 1;
  entry->nx = 0;
  entry->addr = ADDR2TABLE(INIT_PAGE_PDT);

  pd = (struct page_directory *)((uint64_t)pdpt + PAGE_SIZE);
  paddr = ept_walk(INIT_PAGE_PDT, peptp, 0, 0, 0);
  attach_page((uint64_t)pd, paddr, PG_RW);
  kmemset(pd, 0, PAGE_SIZE);
  entry = &(pd->entries[0]);
  entry->present = 1;
  entry->rw = 1;
  entry->nx = 0;
  entry->addr = ADDR2TABLE(INIT_PAGE_PT);

  pt = (struct page_table*)((uint64_t)pd + PAGE_SIZE);
  paddr = ept_walk(INIT_PAGE_PT, peptp, 0, 0, 0);
  attach_page((uint64_t)pt, paddr, PG_RW);
  kmemset(pt, 0, PAGE_SIZE);
  for(i = 0; i < 512; i++) {
    page = &(pt->entries[i]);
    page->present = 1;
    page->rw = 1;
    page->nx = 0;
    page->addr = ADDR2TABLE(PAGE_SIZE*i);
    page->global = 1;
  }

  vmem_free_temp((uint64_t*)pml4t, 4*PAGE_SIZE);
  vkdirty(vk_heap, pml4t, 4);

  return;
}

/* TODO MO: This is very broken */
void switch_vprocs() {

  return;
}

int load_vproc(vproc_t *vm) {
  int rc;

  /*if the vmcs has not been previously loaded, it must be cleared */
  if(vm->active == 1)
  {
      if((rc = vmclear(vm->vmcs_ptr_phys))) {
      kputs("[Hypv] error setting up vmcs vmclear\n");
      panic();
      }	
  }
  rc = vmptrld(vm->vmcs_ptr_phys);
  vm->active = 1;
  if(rc)
    vm->active = 0;
	
	/*if it didn't go off the rails, actually write the updates to the vmcs mem*/
	if(!rc)
	  write_vmcs(&vm->vmcs);
	
  return rc;
}

uint64_t launch_vproc(vproc_t *vm) {
  struct vmcs_mod *mod;
  uint64_t result;
  int rc;

  rc = vmptrld(vm->vmcs_ptr_phys);
  while((mod = qget(vm->vmcs_mods)) != NULL) {
    result = vmwrite(mod->field, mod->value);
    kfree_track(HYPV_SITE,mod);
    if(result) 
      return result;
  }

  vmwrite(HOST_RSP,  vcpu_ptr_array[this_cpu()]->stack);

  run_vproc(&(vm->launched));

  /* If we made it here, there was an error launching. */
  (void)vmread(VM_INSTRUCTION_ERROR, &result);
  return result;
}

/******************vproc queue management stuff ****************/
void vproc_put_idle(vproc_t *vp) {
  qput(vprocs_idle, vp);
  return;
}

void vproc_put_all(vproc_t *vp) {
  qput(vprocs_all, (void *)vp);
  return;
}

static int vproc_search_vpid(void *vproc, const void *lookup)
{
  vproc_t *vp = (vproc_t *)vproc; 
  int *key = (int *)lookup;

  if(vp->vproc_id == *key)
     return 1;
  else
    return 0;    
}

vproc_t* vproc_get(int vproc_id)
{
  vproc_t *vp =  (vproc_t*)qsearch(vprocs_all, vproc_search_vpid, &vproc_id);
                                      
  if(vp != NULL)
    return vp;
  else
    return NULL;
                                      
}
/*
 *I really don't want these functions but the processor only loads the 
 *vmcs_ptr into memory in vmexit routine we don't have a way to 
 *extract the vpid until we've looked it up by the vmcs_ptr address
 */
 
static int vproc_search_vmcsphys(void *vproc, const void *lookup)
{
  vproc_t *vp = (vproc_t *)vproc; 
  int *key = (int *)lookup;

  if(vp->vmcs_ptr_phys == *key)
    return 1;
  else
    return 0;    
}

vproc_t * vproc_getby_vmcs(uint64_t vmcs_ptr_phys)
{

  vproc_t *vp = (vproc_t*)qsearch(vprocs_all, vproc_search_vmcsphys, &vmcs_ptr_phys); 
  
  if(vp != NULL)
    return vp;
  else
    return NULL;  
}

void vproc_remove(void * queue, int vproc_id)
{
  qremove(queue,vproc_search_vpid, &vproc_id); 
  
 return;  
} 

void setup_vproc_management() {
  vprocs_idle = qopen();
  vprocs_all  = qopen();
  
  return;
}

/****************************************************************/

