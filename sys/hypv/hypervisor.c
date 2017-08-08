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
#include <interrupts.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <memory.h>
#include <pci.h>
#include <sbin/vgad.h>
#include <ramio.h>
#include <pio.h>
#include <stdint.h>
#include <acpi.h>
#include <disklabel.h>                       
#include <file_abstraction.h>
#include <vmx.h>
#include <ktimer.h>
#include <pes.h>
#include <fatfs.h>
#include <acpi.h>
#include <apic.h>
#include <smp.h>
#include <vproc.h>
#include <kvmem.h>
#include <vmexit.h>
#include <vmx_utils.h>

extern void systick_asm();
extern void keyboard_asm();
extern void network_asm();

static void hinit();
void systick_hypv_handler(unsigned int vec);
void keyboard_hypv_handler(unsigned int vec);
void network_hypv_handler(unsigned int vec);

#ifdef SERIAL_OUT
////////////////////////////////serial init ///////////////////
#define PORT 0x3f8   /* COM1 */
 
static void init_serial() {
  outb(PORT + 1, 0x00);    // Disable all interrupts
  outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
  outb(PORT + 0, 0x01);    // Set divisor to 3 (lo byte) 115200 baud
  outb(PORT + 1, 0x00);    //                  (hi byte)
  outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
  outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
  outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}
#endif

#ifdef ENABLE_SMP
static void hypv_ap_core_init(void);
int bsp_ready __attribute__ ((section (".bss"))); /* BSP booted */
#endif

/* Just a reminder of where we are at this point:
 * -> We've just been loaded from the stage2 bootloader.
 * -> We're in long mode.
 * What we want to do is:
 * -> Set up our own interrupts and GDT.
 * -> Load the kernel boot2 
 */
void hentry() {

  asm volatile("cli");                     /* Disable interrupts. */
  uint64_t vaddr, paddr, end_addr, frame_array_address, heap_start;

#ifdef ENABLE_SMP
  if(bsp_ready == 1) {
    asm volatile("movq %0,%%rsp  \n\t"
		 "movq %%rsp,%%rbp   \n\t"
		 "movq %1,%%rax      \n\t"
		 "jmp *%%rax"
		 ::"p"(system_stack_base),
		  "p"(hypv_ap_core_init));
  }

  bsp_ready = 1;
#endif

#ifdef SERIAL_OUT
  /*Initialize the serial port for printing */
  init_serial();
#endif

  kprintf("[Hypervisor] Starting\n");

  /* Memory setup. This turns on the general allocator, and initializes
   * the small-structure heap. */

#ifdef DEBUG
  kputs("[Hypervisor] Starting memory initialization.");
#endif

  /* frame_array_address is in rcx; heap_start in rdx */
  asm volatile("movq %%r10, %0" : "=r"(frame_array_address));
  asm volatile("movq %%r11, %0" : "=r"(heap_start));
  asm volatile("movq %%r12, %0" : "=r"(system_stack_base));

  seed_vmem_layer( frame_array_address, heap_start );

  if(!kheapcheck(0,"hypv main")) {
    kputs("[Hypervisor] Error: Invalid heap after memory setup.");
    panic();
  }

  /* map in vga memory */
  vga_set_loc(LEGACY_VGA_MEM);
	
  vaddr = (uint64_t)LEGACY_VGA_MEM;
  paddr = (uint64_t)PHYS_VGA_MEM;
  end_addr = (uint64_t)(LEGACY_VGA_MEM + VGA_MEM_SIZE);
  for( ; vaddr < end_addr; vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
    attach_page(vaddr, paddr, KMEM_IO_FLAGS);
  }

  /* Initialize VK module */
  vmem_alloc( idx2vaddr(23,0,0,0), PAGE_SIZE, 
		   PG_RW | PG_NX | PG_GLOBAL );

  vk_heap = idx2vaddr(23, 0,0,0);

  vkinit(vk_heap, 1000, PAGE_SIZE);

#ifdef VK_DEBUG
  vkshowmap(vk_heap);
#endif

  hinit();

  kputs("[Hypervisor] Error: hmain2 returned. System halting.");
  panic();

  return;
}

#ifdef ENABLE_SMP
void hypv_ap_core_init(void){
  asm volatile("movq %0,%%rsp" : :"r"(temp_stack));  

  vcpu_ptr_array[this_cpu()]->stack = (uint64_t)temp_stack;

#ifdef DEBUG
  kprintf("[SMP] core %d initializing at stack 0x%x\n",this_cpu(), temp_stack);
#endif

  pes_init();

  init_new_gdt(); 
  lapic_init();
  ap_lidt();		 /* loads idt and then loads new tss into gdt*/

#ifdef DEBUG
  kprintf("[HYPV SMP] application core init done\n");
#endif
  set_running();

  /*start interrupts and wait for a guest to need us*/
  asm volatile("sti");
  asm volatile("hlt");

  kprintf("[HYPV PANIC] Major failure in ap core\n");
  asm volatile("hlt");
  return;
}

void ap_join_guest(){
  vproc_t *vp;
  int rc;

  asm volatile("cli");

  lapic_eoi();

  acquire_lock(sem_hypv);

#ifdef DEBUG_SMP    
  kprintf("[HYPV SMP] ap core %d initiliazing\n", this_cpu());
#endif

  /*Init the vmxon_region for each core and store it in the vcpu_t structure*/
  vcpu_ptr_array[this_cpu()]->vmxon_region_virt =(uint64_t)vkmalloc(vk_heap,1);
  
  vmem_alloc((uint64_t*)vcpu_ptr_array[this_cpu()]->vmxon_region_virt, 
	     PAGE_SIZE, PG_RW | PG_GLOBAL);
 
  /*This does some vmx support tests and calls vmxon */
  hypv_entry(vcpu_ptr_array[this_cpu()]->vmxon_region_virt);

  vcpu_ptr_array[this_cpu()]->assigned = 1;

  vp = join_to_vproc(SIPI_vp);

#ifdef DEBUG_SMP  
  kprintf("[HYPV SMP] ap VMCS initialized\n");
#endif

  vcpu_ptr_array[this_cpu()]->assigned = 1;

#ifdef DEBUG_SMP
  kprintf("[HYPV SMP] load vproc\n");
#endif

  rc = load_vproc(vp);
  if (rc != 0)
    kprintf("load_vproc return code %d.\n", rc);

#ifdef DEBUG_SMP
  kprintf("[HYPV SMP] launch vproc\n");
#endif

  rc = launch_vproc(vp);

  /* Unreachable, unless there was an error launching. */
  kprintf("error; return code %d.\n", rc);
  kputs("Halting now.");
  asm volatile("hlt");

  return;
  
}
#endif

static void hinit() {
  int rc, i;
  vproc_t *vp;

#ifdef ENABLE_SMP   
  sem_hypv = create_semaphore( 0 );
  acquire_lock(sem_hypv);

  sem_mult_vm = create_semaphore( 0 );
  release_lock(sem_mult_vm);
#endif

  scan_rsdp();
  Scan_ACPI();
#ifdef DEBUG
  kprintf("[Hypervisor] ACPI Revision %d; RsdtAddress = %x; XsdtAddress = %x\n",
	  rsdpdesc.Revision, rsdpdesc.RsdtAddress, rsdpdesc.XsdtAddress);
#endif

#ifdef SMP_DEBUG
  kprintf("[SMP] apicaddr phys = %x\n",lapicaddr);
#endif

  if(( rc = attach_page(lapicaddr,lapicaddr, KMEM_IO_FLAGS_UC))) {
    kprintf("[SMP] failed to add page location of APIC");
    asm volatile("hlt");
  }  	

  lapic_init();

  if((rc = attach_page(ioapicaddr,ioapicaddr, KMEM_IO_FLAGS_UC)))
    {
      kprintf("[SMP] failed to add page location of IOAPIC");
      asm volatile("hlt");
    }  
  ioapic_init();
  hypv_intr_init();
	
  intr_update_idtentry(0x20, INTR64_ON, (uint64_t)systick_asm);
#ifdef ENABLE_SMP
  intr_update_idtentry(HYPV_START_VP, INTR64_ON, (uint64_t)start_vp);
  intr_update_idtentry(HYPV_PSEUDO_SIPI, INTR64_ON, (uint64_t)ap_join_guest);
#endif
  
  /*create an array the length of cores on the system to store local register*/
  /*context for each core                                                    */
  vcpu_ptr_array = (vcpu_t**)kmalloc_track(HYPV_SITE,
					 smp_num_cpus*sizeof(vcpu_t*));
  
  kmemset(vcpu_ptr_array, 0, smp_num_cpus*(sizeof(vcpu_t*)));

 for( i = 0; i < smp_num_cpus; i++){
   /*This is not a long term solution. As guests are swapped we will need to */
   /*move some varying length/amount of data into the vproc. As of now though*/
   /*no guests are swapped accross cores and each core can maintain it's own */
   /*fiefdom for book keeping                                                */
   vcpu_ptr_array[i] = (vcpu_t*)kmalloc_track(HYPV_SITE, sizeof(vcpu_t));
   vcpu_ptr_array[i]->reg_storage.sse = pes_new_save(); 

   /*By default we assign the BSP to the first guest */
   vcpu_ptr_array[i]->assigned = (this_cpu() == i ? 1 : 0); 
 }

 /*Init the vmxon_region for each core and store it in the vcpu_t structure*/
 vcpu_ptr_array[this_cpu()]->vmxon_region_virt =(uint64_t)vkmalloc(vk_heap,1);

 vmem_alloc((uint64_t*)vcpu_ptr_array[this_cpu()]->vmxon_region_virt, 
	    PAGE_SIZE, PG_RW | PG_GLOBAL);
  
 vcpu_ptr_array[this_cpu()]->stack = system_stack_base;
  /* Scan the PCI bus, then init disk driver */
  pci_init();
	
#ifdef DEBUG
  kprintf("[Hypervisor] Setting up filesystem...\n");
#endif
  file_init();
#ifdef DEBUG
  kprintf("[Hypervisor] Filesystem is up\n");
#endif

  ktimer_init();

  /* Initialize defaults, turn on virtualization. */
#ifdef DEBUG
  kprintf("[Hypervisor] Enabling virtualization features\n");
#endif

  init_vmcs_defaults();

  /*This does some vmx support tests and calls vmxon */
  hypv_entry(vcpu_ptr_array[this_cpu()]->vmxon_region_virt);
	
#ifdef DEBUG
  kprintf("[Hypervisor] Virtualization features setup successful\n");
#endif

  /* open queues for maintaining the vprocs. */
  setup_vproc_management();

#ifdef ENABLE_SMP
  smp_boot_aps();
#endif

  /* Create a new virtual process, and load the kernel into it. */
#ifdef DEBUG
  kprintf("[Hypervisor] creating vproc...\n");
#endif

  vp = create_vproc("kboot2");

  load_vproc(vp); 
  
#ifdef DEBUG
  kprintf("[Hypervisor] Vproc created \n");
#endif

#ifdef DEBUG
  uint64_t cr4;
  cr4 = read_cr4();
  kprintf("[Hypv] CR 4 is 0x%x \n", cr4 );
#endif 

  rc = load_vproc(vp);
  if (rc != 0)
    kprintf("load_vproc return code %d.\n", rc);

#ifdef DEBUG_SMP
  kprintf("[HYPV SMP] launch vproc\n");
#endif

  release_lock(sem_hypv);
  rc = launch_vproc(vp);

  /* Unreachable, unless there was an error launching. */
  kprintf("error; return code %d.\n", rc);
  kputs("Halting now.");
  asm volatile("hlt");
}

void systick_hypv_handler(unsigned int vec) {

  /* MO: Eventually, this will increment the timer */
  return;
}
	
void network_hypv_handler(unsigned int vec) {
  /* MO: Eventually, this will do something ? */

  return;
}
	
void keyboard_hypv_handler(unsigned int vec) {
  int code;
  int val;
  /* Keyboard communication */
#define KB_DATA 0x60	  /* For receiving keyboard scancodes */
#define KB_CTRL 0x64	  /* For acknowledging keys */
#define KBIT	0x80	  /* bit used to ack characters to keyboard */
  /* Send a message to the kernel asking for the character from the KB */
	
  code = inb(KB_DATA);        /* get the scan code for the key struck */
  val = inb(KB_CTRL);         /* strobe the keyboard to ack the char  */
  outb(KB_CTRL, val | KBIT);  /* strobe the bit high                  */
  outb(KB_CTRL, val);	      /* now strobe it low                    */

}


