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
#include <kstring.h>
#include <constants.h>
#include <kload.h>
#include <proc.h>
#include <memory.h>
#include <file_abstraction.h>
#include <ffconf.h>
#include <procman.h>
#include <ksched.h>
#include <elf_loader.h>
#include <apic.h>
#include <kmalloc.h>
#include <kmalloc_sites.h>

#ifdef DIVERSITY
#include <diversity.h>
#endif

#ifdef ENABLE_SMP
#include <smp.h>
#endif


#ifdef DIVERSITY

#define HASHPRTLEN 4
static void print_hash(char *name, unsigned char hash[]) {
  int idx;

  kprintf("[%s\tHash: ", name);
  for(idx=0; idx<32 ; idx++) {
    if(idx<HASHPRTLEN || idx>=(32-HASHPRTLEN))
      kprintf("%x", hash[idx]);
    if(idx==HASHPRTLEN) 
      kprintf(" ... ");
  }
  kprintf("]");

  return;
}
#endif


void kload_daemon(char *dname,int dpid, int ioflags) {
  Proc_t *dp;

  kprintf("[KERNEL] %s\n", dname);
  dp = new_proc(USR_TEXT_START, ioflags ? PL_3 : PL_0, dpid,NULL);

  setprocname(dp, dname);
  dp->is_in_mem = 0;

  ksched_add(dp);		/* add it to the scheduling queue */
  
  return;
}


int load_elf_proc(void) {

  Proc_t *proc; /* Currently running process */

  struct Elf64_Phdr *phdr;
#ifndef DIVERSITY
  uint64_t entry_point;
  struct Elf_Sym* sym;
#endif
  int rc;
  int i;

  uint64_t env_ptr, argv_ptr_array;

#ifdef DIVERSITY
  SHA256_CTX ctx;
  unsigned char hash[32];
#endif

    proc = ksched_get_last();

  /* Open file */
  if(!(proc->is_in_mem)) {
    /* go to the RAM disk */
    proc->file = alloc_elf_ctx(file_read, file_seek, file_error_check);
    proc->file->file_ctx = file_open(proc->procnm);
    if((rc = file_error_check(proc->file->file_ctx))) {
      kprintf("[Kernel] Error opening file %s; error code %d.\n", 
	      proc->procnm, rc);
      return -1;
    }
  } else {
    /* The file has been placed into memory, provide the appropriate
     * memory I/O functions
     */
    proc->file = alloc_elf_ctx(mem_read, mem_seek, mem_error_check);
    proc->file->file_ctx = mem_open();
  }


  /* Read header; find the executable part we need to load.
   * FIXME: In the future, we may have more than one executable segment,
   * which will make the code fall down. (For example, if we ever get
   * library support). For now, we just use the first.
   */
  rc = elf_load_metadata(proc->file);

  if(rc) {
    kprintf("[kernel] Error opening file header for %s.", proc->procnm);
    return -1;
  }

  phdr = proc->file->program_headers;
  if(phdr == NULL) {
    kprintf("[kernel] Error reading file header for %s.", proc->procnm);
    return -1;
  }
  
  /* Load the code. */
  if(!(proc->is_in_mem))
    file_seek(proc->file->file_ctx, 0);
  else
    mem_seek(proc->file->file_ctx, 0);

  proc->mc.rsp = (reg_t)(phdr->p_vaddr - sizeof(uint64_t));
  proc->mc.rbp = proc->mc.rsp;

  /*set the process entry point to the value from elf */
  proc->mc.rip = proc->file->file_header.e_entry;

#ifndef DIVERSITY

  /** loop through the elf symbol table to find the sbrk_end value that is
      set in the usr.bin linker script. */
  for(i = 0, sym = proc->file->symtab; 
      i < proc->file->num_syms; 
      i++, sym++) {

    /* Make sure it's a valid diversity symbol (must be contained 
       in a valid section / have a valid section header). */
    if ( !kstrncmp((char*)ELF_SYMNAME(*sym, proc->file->strtab), 
		   "sbrk_end", kstrlen("sbrk_end") )){

      /* add the heap to the proc's memory region queue */
      add_memory_region(proc, HEAP_REGION, PG_USER | PG_RW | PG_NX, 
			sym->st_value, sym->st_value);
    }
  }

  entry_point = elf_load_file(proc, proc->file);

  if(entry_point == MEM_FAIL) {
    kprintf("[kernel] Error loading elf file %s.\n",proc->procnm);
    /* FIXME: Clean up? */
    return -1;
  }
  /** allocate the stack for the user process */
  vmem_alloc((uint64_t*)((uint64_t)phdr->p_vaddr - 
			 (USR_STACK_PAGES*PAGE_SIZE)),
             USR_STACK_PAGES*PAGE_SIZE, PG_NX | PG_RW | PG_USER);

  add_memory_region(proc, STACK_REGION, PG_USER | PG_RW | PG_NX, 
		    (uint64_t)phdr->p_vaddr - (USR_STACK_PAGES*PAGE_SIZE),
		    phdr->p_vaddr);

#else

  kmemset(hash, 0, 32);
  sha256_init(&ctx);
  /* Special thing we need to do before the start of diversification: 
   * we have to create a diversity unit that represents the stack. 
   * Otherwise, the stack won't get relocated to the new memory space and 
   * the stack pointer won't get updated. Also, it won't be randomized 
   * if we don't do this. 
   */
  struct diversity_unit *stackunit = alloc_diversity_unit(NULL);
  stackunit->addr = (uint64_t)phdr->p_vaddr - (USR_STACK_PAGES*PAGE_SIZE);
  stackunit->memsz = USR_STACK_PAGES * PAGE_SIZE;
  stackunit->hdr = kmalloc_track(KLOAD_SITE, sizeof(struct Elf_Shdr));
  stackunit->hdr->sh_addralign = 16;
  list_add_tail(&proc->file->diversity_units, &stackunit->list);
  proc->file->num_diversity_units++;

  struct diversity_unit *heapunit = alloc_diversity_unit(NULL);
  heapunit->addr = (uint64_t)0x4F000000000;
  heapunit->memsz = 0x30000000;
  heapunit->hdr = kmalloc_track(KLOAD_SITE, sizeof(struct Elf_Shdr));
  heapunit->hdr->sh_addralign = PAGE_SIZE;
  list_add_tail(&proc->file->diversity_units, &heapunit->list);
  proc->file->num_diversity_units++;

  diversify(proc, &ctx);
  sha256_final(&ctx, hash);
  print_hash(proc->procnm, hash);
  kprintf(" @ %x\n",proc->mc.rip);
  kfree_track(KLOAD_SITE, stackunit->hdr);
  kfree_track(KLOAD_SITE, heapunit->hdr);
#endif

  /* Clean up. */
  if(!(proc->is_in_mem))
    file_close(proc->file->file_ctx);
  else {
    kfree_track(KLOAD_SITE,(((void*)(*(uint64_t*)BINARY_LOCATION))));
    mem_close(proc->file->file_ctx);
  }

  /* free up the elf meta data */
  free_elf_ctx(proc->file);

  proc->mc.rsp -= PAGE_SIZE;
  proc->mc.rcx = proc->mc.rsp;

  env_ptr = proc->mc.rcx;

  if ( proc->env ) {
    kmemcpy((void*)env_ptr, (void*)proc->env, PAGE_SIZE);

    /*fixing offsets from array in kmalloc space to a user space addrss*/
    for ( i = 0; i < proc->envc; i++ ) {
      ((uint64_t*)env_ptr)[i] -= proc->env;
      ((uint64_t*)env_ptr)[i] += (uint64_t)proc->mc.rcx;
    }
  }
  else
    kmemset((void*)env_ptr, 0, PAGE_SIZE);

  proc->mc.rdx = proc->envc;

  /* set the end to zero */
  proc->mc.rsp -= sizeof(char*);
  *(uint64_t*)proc->mc.rsp = 0x0;

  /* set up the argv array */
  /* leave space for null terminator */
  proc->mc.rsp -= sizeof(char*)*(proc->argc + 1); 
  proc->mc.rsi = proc->mc.rsp;

  argv_ptr_array = proc->mc.rsi;

  for ( i = 0; i < proc->argc; i++ ) {
    /* Copy the string on the stack */
    proc->mc.rsp -= kstrlen(proc->argv[i]) + 1;
    kstrncpy((char*)(proc->mc.rsp), proc->argv[i], 
	     kstrlen(proc->argv[i]) + 1);
    *(uint64_t*)((uint64_t)argv_ptr_array + (i*sizeof(char*))) = 
      proc->mc.rsp;
  }

  proc->mc.rdi = proc->argc;                  /* Put argc in RDI */

  proc->mc.rsp -= sizeof(char*);
  *(uint64_t*)proc->mc.rsp = 0x0;
  proc->mc.rbp = proc->mc.rsp;     /* new base is after the argv stuff */

  
  return 0;
}
