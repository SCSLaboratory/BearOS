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
#include <kstdio.h>
#include <interrupts.h>
#include <kmalloc.h>
#include <kstring.h>
#include <memory.h>
#include <asm_subroutines.h>
#include <stdint.h>
#include <procman.h>
#include <ksched.h>
#include <pio.h>
#include <kvcall.h>
#include <kvmem.h>
#include <apic.h>
#include <smp.h>

#ifdef KERNEL
#include <ksched.h>
#endif

extern void vec0();
extern void vec1();
extern void vec20();
extern void vec21();
extern void vec28();
extern void vec29();
extern void vec80();
extern void vec81();
extern void vec7F();

extern void reload_segs();

#ifndef BOOTLOADER
extern int exception_handler_len;
extern int pic_handler_len;
extern int swi_handler_len;

static void lidt();
static idtentry_t *idt;
void disable_pic();

#endif

#ifdef KERNEL
static uint64_t *dead_ip, dead_count = 0;
#endif

static void get_gdt(struct gdt_desc *gdt);
#ifndef BOOTLOADER
static void init_tss();
static void (**handler_array)(unsigned int, void*);
static void **handler_arg;
#else
void init_tss();
#endif

#ifndef BOOTLOADER
void ap_lidt(){
  lidt();
#ifdef DEBUG_SMP
  kprintf("	[Interrupts]Loading tss for core %d\n",this_cpu());
#endif
  init_tss(); 
}
#endif

#ifndef BOOTLOADER

void hypv_intr_init(){
  int i;
  uint64_t block_base;
  uint64_t block_delta;
  uint64_t func_addr;

  /* Initialize the TSS. */
  /* This is a 104-byte memory location that controls
   * the location of the stack when a true interrupt occurs. */
  init_tss();
  /* Malloc the Interrupt Descriptor Table.  It tells the processor
   * which function to run when a certain IRQ or exception occurs. */
  idt = (idtentry_t *)kmalloc_track(INTERRUPTS_SITE,sizeof(idtentry_t)*IDT_NUM_ENTRIES);
  kmemset(idt,0,sizeof(idtentry_t)*IDT_NUM_ENTRIES);

  /* allocate and Null out handler arrays */
  handler_arg = kmalloc_track(INTERRUPTS_SITE,256*sizeof(void*));
  handler_array = kmalloc_track(INTERRUPTS_SITE,256*sizeof(void*));
  kmemset(handler_arg,0,256*sizeof(void*));
  kmemset(handler_array,0,256*sizeof(void*));
	
  /* Set up IDT with the asm exception functions */
  block_base = (uint64_t)(&vec0);
  block_delta = exception_handler_len;
  for(i = 0, func_addr = block_base;
      i < NUM_EXCEPTIONS;
      i++, func_addr += block_delta) {
    intr_update_idtentry(i,INTR64_ON, func_addr);
  }
	
  /* Tell the processor where the IDT is in memory. */
  lidt();

  /* Initialize the Programmable Interrupt Controller.  This used to be
   * a piece of hardware; it has been deprecated by the APIC (advanced PIC).
   * It is in charge of wrangling the IRQ lines and delivering them to
   * the CPU in an orderly manner. */
  init_pic();
  /* because the PIC can't be truely disabled and will continue to fire
     spurious interrupts. The first step to disabling it is to enable and
     remap the pic interrupts. Then all of the PIC interrupts can be masked off.
     Not doing this causes double faults, general protection faults, etc.
  */

  disable_pic();


}
	
void intr_init() {
  int i;
  uint64_t block_base;
  uint64_t block_delta;
  uint64_t func_addr;

  /* Initialize the TSS. */
  /* This is a 104-byte memory location that controls
   * the location of the stack when a true interrupt occurs. */
  /** This was done with uniprocessing in mind (RMD) */
  init_tss();

  /* Malloc the Interrupt Descriptor Table.  It tells the processor
   * which function to run when a certain IRQ or exception occurs. */
  idt = (idtentry_t *)kmalloc_track(INTERRUPTS_SITE,sizeof(idtentry_t)*IDT_NUM_ENTRIES);
  kmemset(idt,0,sizeof(idtentry_t)*IDT_NUM_ENTRIES);

  /* allocate and Null out handler arrays */
  handler_arg = kmalloc_track(INTERRUPTS_SITE,256*sizeof(void*));
  handler_array = kmalloc_track(INTERRUPTS_SITE,256*sizeof(void*));
  kmemset(handler_arg,0,256*sizeof(void*));
  kmemset(handler_array,0,256*sizeof(void*));

  /* Set up IDT with the asm exception functions */
  block_base = (uint64_t)(&vec0);
  block_delta = exception_handler_len;
  for(i = 0, func_addr = block_base;
      i < NUM_EXCEPTIONS;
      i++, func_addr += block_delta) {
    intr_update_idtentry(i,INTR64_ON, func_addr);
  }

  /* Set up IDT with asm master PIC functions */
  block_base = (uint64_t)(&vec20);
  block_delta = pic_handler_len;
  for(i = 0x20, func_addr = block_base;
      i < 0x28;
      i++, func_addr += block_delta) {
    intr_update_idtentry(i, INTR64_ON, func_addr);
  }
	
  /* Set up IDT with asm slave PIC functions */
  block_base = (uint64_t)(&vec28);
  block_delta = pic_handler_len;
  for(i = 0x28, func_addr = block_base;
      i < 0x30;
      i++, func_addr += block_delta) {
    intr_update_idtentry(i,INTR64_ON, func_addr);
  }

  /* Set up IDT with asm master PIC functions */
  block_base = (uint64_t)(&vec7F);
  block_delta = swi_handler_len;
  for(i = 0x7F, func_addr = block_base;
      i < 0x90;
      i++, func_addr += block_delta) {

    if(i == 0x81)
      intr_update_idtentry(i, INTR64_ON, func_addr);
    else
      intr_update_idtentry(i, INTR64_ON, func_addr);
  }


  /* Tell the processor where the IDT is in memory. */
  lidt();

  /* Initialize the Programmable Interrupt Controller.  This used to be
   * a piece of hardware; it has been deprecated by the APIC (advanced PIC).
   * It is in charge of wrangling the IRQ lines and delivering them to
   * the CPU in an orderly manner. */
  init_pic();
  /* because the PIC can't be truely disabled and will continue to fire
     spurious interrupts. The first step to disabling it is to enable and
     remap the pic interrupts. Then all of the PIC interrupts can be masked off
     Not doing this causes double faults, general protection faults, etc.
  */

  disable_pic();

  return;
}

/* Load the IDT */
static void lidt() {
  struct idtptr idtp;
  idtp.limit = IDT_NUM_ENTRIES * sizeof(idtentry_t) - 1;
  idtp.base = (uint64_t)idt;
#ifdef DEBUG
  kprintf("	[Interrupts] IDT location is 0x%x\n", (uint64_t)idt);
#endif
  /*
  int vec;
  kprintf("old idt\n");
  for ( vec = 0; vec < idtp.limit + 1; vec += 8 )
    kprintf("  0x%x\n", *(uint64_t*)(((uint64_t)idtp.base)+vec));
  */


  asm volatile("lidt %0" :: "m"(idtp));

  return;
}
#endif

#ifndef BOOTLOADER
void intr_invoke_handler(uint64_t vector) {
  void (*func)(unsigned int, void*);

  if (vector > 256) {
    kprintf("	[Interrupts] ERROR: Something horrible happened vector = %x\n",vector);
    return;
  }

  func = handler_array[vector];

  if (func == NULL) {
    kprintf("	[Interrupts] ERROR: Interrupt %x has no registered handler.\n", vector);
    return;
  }

  func(vector,handler_arg[vector]);

  return;
}
#endif

#ifndef BOOTLOADER
void intr_add_handler(uint8_t i, void (*func)(unsigned int, void*),
		      void *arg) {
  handler_array[i] = func;
  handler_arg[i] = arg;

  return;
}
#endif

#ifndef BOOTLOADER
void intr_update_idtentry(uint8_t i, int type, uint64_t func) {
  kmemset(idt+i, 0, sizeof(idtentry_t));
  (idt+i)->type = type;
  (idt+i)->selector = 0x08;           /* FIXME: get rid of magic number */
  (idt+i)->ist = 0x1;                 /* Always 0x1 */
  idt_set_offset(idt+i, func);
}
#endif


/* Mallocs a new GDT, copying the values in the current one to
 * maintain our segment descriptors. */
#ifndef BOOTLOADER
void init_new_gdt() {
  struct gdt_desc gdt;
  struct gdt_desc newgdt;
  get_gdt(&gdt);
  /* Add 4096*512*512*512 to the GDT to transform from physical to virtual
   * address. This has to be done because the GDT was initially created in
   * identity-mapped addressing, and then that whole region was moved up to
   * the next PML4T entry. This means this function can ONLY be called when
   * the old GDT was created in the initial, identity-mapped addressing. */
  //gdt.base += PML4T_ENTRY_1;
#ifdef DEBUG
  kprintf("	[Interrupts] gdt.limit = %d decimal\n", gdt.limit);
#endif
  newgdt.base = (uint64_t)kmalloc_track(INTERRUPTS_SITE,(gdt.limit)+1);
#ifdef DEBUG
  kprintf("	[Interrupts] newgdt.base = 0x%x \n", newgdt.base);
#endif
  newgdt.limit = gdt.limit;
  kmemcpy((void *)(newgdt.base),(void *)(gdt.base),(gdt.limit)+1);
  /* Hack: Update the task register to be "available" in case the one
   * previously put in the bootloader is now "busy". */
  Tssdesc_t *te = (Tssdesc_t *)(newgdt.base + 0x28);
  te->c5 = (uint8_t)0x89;
  asm volatile("lgdt %0" : "=m" (newgdt));
  reload_segs();
}
#endif

/* Sets a bit in the interrupt bitmap mask, keeping that irq from firing. */
#ifndef BOOTLOADER
void intr_set_mask(unsigned char irq) {
  uint16_t port;
  uint8_t value;

  if (irq < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq -= 8;
  }
  value = inb(port) | (1 << irq);
  outb(port, value);
}
#endif

/* Clears a bit in the interrupt bitmap mask, allowing that irq to fire. */
#ifndef BOOTLOADER
void intr_clear_mask(unsigned char irq) {
  uint16_t port;
  uint8_t value;

  if(irq < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq -= 8;
  }
  value = inb(port) & ~(1 << irq);
  outb(port, value);
}
#endif

/* Put offset to an interrupt service routine into the IDT */
void idt_set_offset(idtentry_t *interrupt, uint64_t addr) {
  interrupt->base_low = (uint16_t)(addr & 0xFFFF);
  addr >>= 16;
  interrupt->base_mid = (uint16_t)(addr & 0xFFFF);
  addr >>= 16;
  interrupt->base_high = (uint32_t)(addr & 0xFFFFFFFF);
}

/* Initialize the pic chip; move IRQ's out of 
 * system interrupt/exception range. */
void init_pic() {
  /* Start the initialization sequence */
  outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
  io_wait;
  outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
  io_wait;
  /* Define the PIC vectors */
  outb(PIC1_DATA, PIC_OFFSET1);
  io_wait;
  outb(PIC2_DATA, PIC_OFFSET2);
  io_wait;
  /* Continue initialization sequence */
  outb(PIC1_DATA, 4);
  io_wait;
  outb(PIC2_DATA, 2);
  io_wait;
 
  outb(PIC1_DATA, ICW4_8086);
  io_wait;
  outb(PIC2_DATA, ICW4_8086);
  io_wait;
	
  outb(PIC1_DATA,PIC1_MASK);
  io_wait;
  outb(PIC2_DATA,PIC2_MASK);
}


void disable_pic(){
  outb(0xa1,0xff);
  io_wait;
  outb(0x21,0xff);
  io_wait;
}

/* Gets the location/size of the GDT from the processor control structures. */
static void get_gdt(struct gdt_desc *gdt) {
  asm volatile("sgdt %w0" : : "m"(*gdt) : "memory");
}

/* Gets the TSS entry out of the GDT. */
static uint8_t *get_tss_entry() {
  struct gdt_desc gdt;
  get_gdt(&gdt);
  uint64_t tssaddr = gdt.base + TSS_ENTRY_OFFSET;
  return ((uint8_t *)tssaddr);
}

/* Update the TSS' entry in the GDT to point to a new TSS. */
static void update_tss_entry(tss_t *tss) {
  uint64_t tss_addr = (uint64_t)tss;
  Tssdesc_t *te = (Tssdesc_t *)get_tss_entry();
	
  /* Update Address */
  te->c2 = (uint8_t)(tss_addr & 0xFF);
  te->c3 = (uint8_t)((tss_addr >> 8) & 0xFF);
  te->c4 = (uint8_t)((tss_addr >> 16) & 0xFF);
  te->c7 = (uint8_t)((tss_addr >> 24) & 0xFF);
  te->upper_addr = (uint32_t)((tss_addr >> 32) & 0xFFFFFFFF);
  te->zeros = 0x0;
	
  /* Update type, just in case something has made it
     "busy" instead of "available" */
  te->c5 = (uint8_t)0x89;
}

/* Create a new Task State Segment.  This is a strange Intel structure
 * that has done many different jobs over the years.  At this point, the
 * only thing it does is tell the processor where the kernel stack(s)
 * is/are for a true interrupt. 
 */
#ifndef BOOTLOADER
static void init_tss() {
  tss_t *tss_base;
  tss_t *tss;
  int i;
  uint64_t stk;

  /* Allocate stuff */
#ifndef HYPV
  stk = (uint64_t)system_stack_base; /* the TSS stack IS the kernel stack */
#else
  stk = (uint64_t)new_stack(NULL);
#ifdef DEBUG
  kprintf("[HYPV INT] system stack 0x%x new stack 0x%x\n",
	  (uint64_t)system_stack_base, stk);
#endif
#endif
  tss_base = (tss_t*)kmalloc_track(INTERRUPTS_SITE,TSS_SIZE);
  tss = tss_base;

  /* Write TSS so that all interrupts will run on stk */
  kmemset((void *)(tss),0,4); // First 4 bytes are reserved
  tss = (tss_t *)((uint64_t)(tss) + 4);
  for(i=0;i<12;i++) {
    if ((i == 3) || (i == 11)) {
      (tss+i)->rsp_low  = 0x0;
      (tss+i)->rsp_high = 0x0;
    } else {
      (tss+i)->rsp_low  = (uint32_t)(stk & 0xFFFFFFFF);
      (tss+i)->rsp_high = (uint32_t)((stk >> 32) & 0xFFFFFFFF);
    }
  }
  // Index 12: Bits 0-15 reserved; 16-31 = offset to IO permission bitmap
  // IO perm. map is outside of the segment, meaning all perms granted
  (tss+12)->rsp_low = 0x68 << 16;

  /* Put addr of TSS into GDT */
  update_tss_entry(tss_base);

  /* Load the TSS into the processor's task register */
  asm volatile("movw $0x28, %%ax\n\t"
	       "ltr %%ax"
	       ::: "%ax");
	
  return;
}
#else /* Compiling for the bootloader */
void init_tss(uint64_t stk, tss_t *tss_base) {
  tss_t *tss = tss_base;
  unsigned int i;

  /* Write TSS so that all interrupts will run on stk */
  kmemset((void *)(tss),0,4); // First 4 bytes are reserved
  tss = (tss_t *)((uint64_t)(tss) + 4);
  for(i=0;i<12;i++) {
    if ((i == 3) || (i == 11)) {
      (tss+i)->rsp_low  = 0x0;
      (tss+i)->rsp_high = 0x0;
    } else {
      (tss+i)->rsp_low  = (uint32_t)stk;
      (tss+i)->rsp_high = 0x0;
    }
  }
  // Index 12: Bits 0-15 reserved; 16-31 = offset to IO permission bitmap
  // IO perm. map is outside of the segment, meaning all perms granted
  (tss+12)->rsp_low = (0x68 << 16);

  /* Put addr of TSS into GDT */
  update_tss_entry(tss_base);

  /* Load the TSS */
  asm volatile("movw $0x28, %%ax\n\t"
	       "ltr %%ax"
	       ::: "%ax");
}
#endif

#ifdef KERNEL
#ifdef ENABLE_SMP
void interrupt_acquire_lock(void){
	
  acquire_lock(sem_kernel);
}
void interrupt_release_lock(void){

  release_lock(sem_kernel);
}
#endif
#endif

/******************************************************************************
 * EXCEPTION HANDLING *********************************************************
 *****************************************************************************/

/* Print the exception vector and CR2 if this was a pagefault. */
void print_exception_info_one(uint64_t vec) {
  kprintf("\nEXCEPTION ENCOUNTERED\n");
  kprintf("Vector: 0x%x\n",vec);

#ifdef EXCEPTION_DEBUG
  if (vec == 0xE) {		/* page fault */
    kprintf("CR2          : 0x%x\n",read_cr2());
#ifdef KERNEL
    //kprintf("memtrace: ");
    kprintf("core that died = %d\n",this_cpu());
    //release_lock(sem_kernel);
    //  asm volatile("hlt");
    return;
    //trace_mem_access((void*)read_cr2());
#endif
  }
  else if (vec == 0x13) {	/* general protection fault */
    uint32_t mxcsr;
    asm volatile("stmxcsr %0\n\t"
		 : "=m"(mxcsr));
    kprintf("MXCSR        : 0x%x\n", mxcsr);
  }
#ifdef KERNEL 
#ifndef DIVERSITY
  // kvmcall(0,0,NULL);		/* do a stack trace */
#endif	/* DIVERSITY */
#endif	/* KERNEL */
#else  /* NOT EXECTION_DEBUG */

  kprintf("Segmentation Fault\n");
#ifdef KERNEL			/* if in kernel code */
#ifndef DIVERSITY		/* no string table with diversity */
  // kvmcall(0,0,NULL);		/* do a stack trace */
#endif	/* DIVERSITY */
#endif	/* KERNEL */
  asm volatile("hlt");
#endif	/* EXCEPTION_DEBUG */
}

/* 
 * Note: This doesn't work right when an exception is triggered with an
 * "int x" instruction.  In that case the processor does not push an error
 * code on the stack, so these will all be off by one, and the stack segment
 * will be junk.  Either that or it simply won't work because the stack is not
 * aligned as it should be.
 */
void print_exception_info_two(uint64_t ss, uint64_t *rsp, uint64_t rflags,
			      uint64_t cs, uint64_t *rip, uint64_t ec) {
#ifdef EXCEPTION_DEBUG
  kprintf("Error Code: 0x%x    ",ec);  
  kprintf("RFLAGS: 0x%x\n",rflags);
 
  kprintf("Code Segment: 0x%x  ",cs);
  kprintf("Stack Segment: 0x%x \n",ss);

  kprintf("Stack Pointer: 0x%x; Blah=0x%x\n", rsp, *(uint64_t*)rsp);
  kprintf("RIP:           0x%x \n",rip);
#ifdef KERNEL
#ifdef ENABLE_SMP
  kprintf("core that died = %d\n",this_cpu());
#endif
#endif
#endif

#ifdef KERNEL
  Proc_t *cp = ksched_get_last();
#ifdef EXCEPTION_DEBUG
  if (cp) kprintf("last Proc PID: %d, %s\n",cp->pid, cp->procnm);
#endif

  /* nifty little trick to prevent endless loops 
   * if the same process keeps faulting, why 7? cause I can
   */
  asm volatile("hlt");		/* not using trick right now */

  if(dead_count > 5){
    kprintf("Warning Unrecoverable Fault System halting\n");
    asm volatile("hlt");
  }
  else if(rip != dead_ip)
    dead_count = 0;
  
  if(rip == dead_ip)
    dead_count++;
  else{
    dead_ip = rip;
    asm volatile("hlt"); /*uncomment if you want to just halt on error */
  }
  /* I didn't want to link syscall in here so destroy proc is called
   * with 8, which is what the kill syscall would send destroy
   * proc. After the faulting process is thouroughly killed. We
   * schedule the next process and run it. (RMD)
   */

  /* TODO Kill ourselves */
  destroy_proc(cp,8);
  /*schedule the next process */

  ksched_yield();

#endif	/* KERNEL */

#ifdef EXCEPTION_DEBUG
  kprintf("See Intel Vol 3A Chapter 6.13/6.15 for description of error code\n");
#endif
}

