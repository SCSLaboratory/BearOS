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
#ifndef _IN_ASM
#include <vmx.h>
#include <stdint.h>
#include <shadow_vmcs.h>
#endif


#ifndef _IN_ASM
/* Vproc structure and related functions. */
typedef struct vproc {
  int launched;
  int active;
  int running;
  int vproc_id;

  /* ELF file details. */
  struct elf_ctx *file;

  /* Memory used by the running process, so we can inspect it. */
  void *memory;
  uint64_t memsz;
  struct page_map_level_4_table *mem_root;
  uint64_t peptp;
  uint64_t virt_apic_page;
  uint64_t virt_apic_access_addr; 
  uint64_t uid;
  /* Pointers to top of memory at bootup time -- used when we allocate things
   * for the vproc. Don't use these after the kernel "boots". */
  uint64_t scratch_current;       /* Offset to top of scratch mem */
  uint64_t kmem_current;          /* Offset to top of kernel mem */
  void *new_base;                 /* Replacement for bottom 8MB */
  uint64_t new_base_current;      /* Offset to top of replacement */

  /* The actual place the vmcs region will end up in memory. */
  void *vmcs_ptr;                 /* Region given by kmalloc */
  uint64_t vmcs_ptr_phys;         /* Physical version for the CPU */
  vmcs_t vmcs;                    /* Local copy of the VMCS params */
  void *vmcs_mods;                /* Modification queue for VMCS changes */
	
  vector pending_interrupts_vec;  /*storage for pending interrupts*/

  struct mcontext reg_storage;
  struct memmap* memmap;          /* BIOS memmap created by hypv */
  uint16_t* memmap_entries;       /* Number of entries in memmap */
  shadow_vmcs_t vmcs_shadow;      /* for nesting shadow the vmcs */ 
} vproc_t;
#endif

vproc_t *create_vproc(char *);
void destroy_vproc(vproc_t *);
void diversify_vproc(vproc_t *);

/* Functions for the vproc queues. */
vproc_t* vproc_get(int vproc_id);
vproc_t * vproc_getby_vmcs(uint64_t vmcs_ptr_phys);
void vproc_put_all(vproc_t *vp); 
void vproc_put_idle(vproc_t * vp);
void vproc_remove(void * queue, int vproc_id);
void setup_vproc_management();

/* Other hypervisor functions. */

int load_vproc(vproc_t *);
uint64_t launch_vproc(vproc_t *);
int get_vpid_cnt();

/*Function to join a core to a vproc*/
vproc_t *join_to_vproc(vproc_t *vp_to_join);

/* ---------------------- Static variables --------------------- */

/*queues to maintain the vprocs in */
void *vprocs_idle; 
void *vprocs_all;
