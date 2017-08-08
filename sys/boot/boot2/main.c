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
#include <disklabel.h>
#include <interrupts.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <memory.h>
#include <stdint.h>
#include <sbin/vgad.h>
#include <ramio.h>
#include <fatfs.h>
#include <bootconstants.h>
#include <bootinterrupts.h>
#include <bootmemory.h>
#include <proc.h>
#include <file_abstraction.h>
#include <fatfs.h>
#include <ramio.h>
#include <elf_loader.h>
#include <elf.h>
#include <asm_subroutines.h>

#ifdef DIVERSITY
#include <sha256.h>
#include <diversity.h>
#endif

/* NOTE ABOUT THIS FILE: 
   Any reference to "kernel" in this file is actually "binaryte": whatever 
   binary this bootloader is bringing up. In other words, it is possible that 
   "kernel" is really the hypervisor.

   In the case where the hypervisor is booted by boot2, the hypervisor 
   launches the kernel by kicking off a second stage 2 bootloader, called 
   KBOOT2. In that case, this code is compiled and run twice. The first time,
   "kernel" is the hypervisor. The second time, it is the actual Bear kernel.
 */
#ifdef KBOOT
#define BINARYTE "/kernel"
#else
#define BINARYTE "/binaryte"
#endif

extern int read(FIL *, FILINFO *, uint64_t *);
#ifdef ENABLE_SMP
int bsp_ready __attribute__ ((section (".bss")));
volatile uint64_t* core_boot_ptr;
#endif

#ifdef DIVERSITY
static void print_hash(unsigned char hash[]);
#endif 

void start() __attribute__ ((section ("startpoint")));

static void fs_error(FRESULT code) {
  int i;
  kprintf("Failed with code %d\n", code);
  for(i=0;i<1000;i++)
    ;
  asm volatile("hlt");
}

#define checkstatus(x) if(x) fs_error(x)

static Proc_t *load_elf_bin( uint64_t fs_offset ) {

  struct Elf64_Phdr *phdr;
  int rc;
  Proc_t *proc;
  FATFS fatfs;
  DIR dir;
  FILINFO stat;
#ifndef DIVERSITY
  uint64_t entry_point;
#endif

#ifdef DIVERSITY
  SHA256_CTX ctx;
  unsigned char hash[32];
#endif

  rc = f_mount_offset(0, &fatfs, fs_offset);
  checkstatus(rc);
#ifdef DEBUG
  kputs("[Boot] Successfully Mounted Disk.");

  rc = f_opendir(&dir, "");
  kputs("dir has been opened");
  if(rc) fs_error(rc);
  kprintf("Directory listing: \n");
  for(;;){
    rc = f_readdir(&dir, &stat);
    if( rc || !stat.fname[0]) break;
    if(stat.fattrib & AM_DIR)
      kprintf("directory %d", stat.fname);
    else
      kprintf(" Name: %s Size: %d  \n", stat.fname, stat.fsize);
  }
  if (rc)
    fs_error(rc);
#endif

  proc = (Proc_t*)kmalloc( sizeof(Proc_t) );

  proc->file = alloc_elf_ctx( file_read, file_seek, file_error_check );
  proc->file->file_ctx = file_open( BINARYTE );
  if(( rc = file_error_check(proc->file->file_ctx)) ) {
    kprintf("[Boot2] Error opening file %s; error code %d.\n", BINARYTE, rc);
    panic();
  }
 
  rc = elf_load_metadata( proc->file );

#ifdef DEBUG
  kputs("[Boot2] Loaded metadata");
#endif

  if( rc ) {
    kprintf("[Boot2] Error opening file header for %s.", BINARYTE);
    panic();
  }

  phdr = proc->file->program_headers;
  if( phdr == NULL ) {
    kprintf("[Boot2] Error reading file header for %s.", BINARYTE);
    panic();
  }

  file_seek( proc->file->file_ctx, 0 );

  proc->mc.rip = proc->file->file_header.e_entry;
  proc->mc.rsp = (reg_t)(phdr->p_vaddr - sizeof(uint64_t));
  proc->mc.rbp = proc->mc.rsp;

  /* tell it the kernel is in the ramdisk */
  proc->is_in_mem = 0;

#ifndef DIVERSITY

  entry_point = elf_load_file(proc->file);

  if(entry_point == MEM_FAIL) {
    kprintf( "[Boot2] Error loading elf file %s.\n", BINARYTE );
    panic();
  }

  /* allocate the kernel stack */
  vmem_alloc((uint64_t*)((uint64_t)phdr->p_vaddr - (KSTACK_PAGES*PAGE_SIZE)),
             KSTACK_PAGES*PAGE_SIZE, PG_NX | PG_RW );
#else
  kmemset(hash, 0, 32);
  sha256_init(&ctx);

  /* Special thing we need to do before the start of diversification: we have  
   * to create a diversity unit that represents the stack. Otherwise, the      
   * stack won't get relocated to the new memory space and the stack pointer   
   * won't get updated. Also, it won't be randomized if we don't do this. 
   */
  struct diversity_unit *stackunit = alloc_diversity_unit(NULL);
  stackunit->addr = (uint64_t)phdr->p_vaddr - (KSTACK_PAGES*PAGE_SIZE);
  stackunit->memsz = KSTACK_PAGES * PAGE_SIZE;
  stackunit->hdr = kmalloc(sizeof(struct Elf_Shdr));
  stackunit->hdr->sh_addralign = 16;
  list_add_tail(&proc->file->diversity_units, &stackunit->list);
  proc->file->num_diversity_units++;

  /* diversify the process! */
  diversify( proc, &ctx );
  sha256_final(&ctx, hash);
  print_hash(hash);                                                        
  kfree(stackunit->hdr);

#endif

  /* Clean up. */
  file_close(proc->file->file_ctx);

  /* free up the elf meta data */
  free_elf_ctx(proc->file);

  proc->mc.rsp -= PAGE_SIZE;
  proc->mc.rcx = proc->mc.rsp;

  /* set the end to zero */
  proc->mc.rsp -= sizeof(char*);
  *(uint64_t*)proc->mc.rsp = 0x0;

  return proc;
}

void start() {
  struct partition *part;
  Proc_t *proc;
  uint64_t fs_offset, frame_array_vaddr, heap_start;


  asm volatile("cli");

  /*DO NOT PUT ANYTHING HERE IF SMP IS BEING USED
    UNLESS EACH CORE NEEDS TO EXECUTE IT, WHICH IT 
    SHOULDN'T*/

#ifdef ENABLE_SMP
  if(bsp_ready == 1)
    {
      asm volatile("movq $0x7bf8,%%rsp\n\t"
		   "movq %0,%%rbx \n\t"
		   "jmp *%%rbx"
		   :: "p"(core_boot_ptr));
    }
  bsp_ready = 1;
#endif
  kprintf("[Boot2]\n");
#ifdef DEBUG
  kputs("[Boot] Entered C entry routine.");
#endif

  bootmemory_init();

  vmem_init();

  /* Load the hypervisor into memory, start using it. */
#ifdef DEBUG
  kputs("[Boot] Loading hypervisor...");
#endif
  fs_offset = *((uint16_t *)SLICE_OFFSET);
  fs_offset *= 512;
  /* Read from the first partition. Might want to change this later to give
   * the user a choice. */
  part = (struct partition *)(SLICE_MBR_START + sizeof(struct disklabel));
  fs_offset += part->boffset;
#ifdef DEBUG
  kprintf("[Boot] Filesystem Offset is %d\n", fs_offset);
#endif
  proc = load_elf_bin(fs_offset);

#ifdef DEBUG
  kprintf("[Boot] Jumping to 0x%x.\n",proc->mc.rip);
#endif
#ifdef ENABLE_SMP
  core_boot_ptr = (uint64_t*)proc->mc.rip;
#endif
  /* Does not return. */

  /* pass the frame array vaddr into the kernel via r10 */
  frame_array_vaddr = get_frame_array_vaddr();
  asm volatile("movq %0, %%r10\n\t"
	       :: "p"(frame_array_vaddr));
  
  /* pass the heap vaddr into the kernel via r11 */
  heap_start = get_heap_start();
  asm volatile("movq %0, %%r11\n\t"
	       :: "p"(heap_start));  

#ifdef KPLT
  asm volatile("movq %0, %%r13\n\t"
	       :: "p"(proc->mc.r13));
#endif

  /* for system_stack_base */
  asm volatile("movq %0, %%r12\n\t"
	       :: "p"(proc->mc.rsp));

  /* set the stack pointer */
  asm volatile("movq %0, %%rsp\n\t"
	       :: "p"(proc->mc.rsp));

  /* set the instruction pointer */
  asm volatile("movq %0,%%rbx \n\t"
	       "jmp *%%rbx"
	       :: "p"(proc->mc.rip));

  /* never execute this. */
  asm volatile("hlt");
}

#ifdef DIVERSITY
static void print_hash(unsigned char hash[]) {
  int idx;

  kprintf("[%s Hash: ", BINARYTE);
  for(idx=0;idx<32;idx++)
    kprintf("%x", hash[idx]);
  kprintf("]\n");

  return;
}
#endif
