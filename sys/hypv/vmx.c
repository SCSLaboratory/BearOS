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
#include <elf_loader.h>
#include <file_abstraction.h>
#include <khash.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <mcontext.h>
#include <memory.h>
#include <interrupts.h>
#include <kqueue.h>
#include <stdint.h>
#include "asm.h"
#include <vmx.h>
#include <pci.h>
#include <ktimer.h>
#include <apic.h>
#include <pes.h>
#include <ke1000.h>
#include <ept.h>
#include <vmx_utils.h>
#include <kvmem.h>
#include <vproc.h>
#include <vmexit.h>
#include <vmx_utils.h>

/* Container that holds the VMX capabilities the hardware allows */
struct vmx_msrs_t vmx_msrs;              /* The VMX-related MSRs */

static int is_supported_nic(Pci_dev_t *dev);

/* MO: TODO Defaults for virtual machines. This should be GUEST SPECIFIC
 * There should be no one specific default.
 */
struct vmcs vmcs_default __attribute__ ((section (".data"))) = {

  .PIN_BASED_VM_EXEC_CONTROL = (1 << PIN_NMI_EXITING),

  .SECONDARY_VM_EXEC_CONTROL = (1 << PROC_SEC_ENABLE_EPT) | 
                               (1 << PROC_SEC_ENABLE_RDTSCP) | 
                               (1 << PROC_SEC_ENABLE_VPID) | 
                               (1 << PROC_SEC_VIRT_APIC_ACCESS) |
                               (1 << PROC_SEC_UNRESTRICTED),

  .CPU_BASED_VM_EXEC_CONTROL = (1 << PROC_PRI_SECONDARY_CTRLS_ON) | 
                               (1 << PROC_PRI_USE_MSR_BITMAP) | 
                               (1 << PROC_PRI_USE_TPR_SHADOW),

  /* 
   * This field is counter intutitive setting bits here PREVENTS 
   * the guest from modifying them 
   */
  .CR0_GUEST_HOST_MASK = 0,
  .CR4_GUEST_HOST_MASK = 0,               
  /* Host segment selectors and control registers. */
  .HOST_CS_SELECTOR = 0x8,
  .HOST_DS_SELECTOR = 0x10,    
  .HOST_SS_SELECTOR = 0x10,
  .HOST_ES_SELECTOR = 0x10,
  .HOST_FS_SELECTOR = 0x10,
  .HOST_GS_SELECTOR = 0x10,
  .HOST_RIP = (uint64_t)vmexit_trap,
  .HOST_CR0 = 0,                           /* filled in later */
  .HOST_CR3 = 0,                           /* filled in later */
  .HOST_CR4 = (1 << 13),(1<<7),            /* filled in later, keep VMX on! */
  /* Processor state for the guest. */
  .GUEST_CS_SELECTOR = 0x8,
  .GUEST_DS_SELECTOR = 0x10,
  .GUEST_SS_SELECTOR = 0x10,
  .GUEST_ES_SELECTOR = 0x10,
  .GUEST_FS_SELECTOR = 0x10,
  .GUEST_GS_SELECTOR = 0x10,
  .GUEST_RIP = 0,                          /* filled in later */
  .GUEST_RSP = 0,                          /* In general */
  .GUEST_CR0 = 0,                          /* filled in later */
  .GUEST_CR3 = 0,                          /* filled in later */
  .GUEST_CR4 = 0,                          /* filled in later */

  /* Guest processor cache. */
  .GUEST_CS_BASE = 0,
  .GUEST_DS_BASE = 0,
  .GUEST_SS_BASE = 0,
  .GUEST_ES_BASE = 0,
  .GUEST_FS_BASE = 0,
  .GUEST_GS_BASE = 0,
  .GUEST_CS_LIMIT = 0xFFFFFFFF,
  .GUEST_DS_LIMIT = 0xFFFFFFFF,
  .GUEST_SS_LIMIT = 0xFFFFFFFF,
  .GUEST_ES_LIMIT = 0xFFFFFFFF,
  .GUEST_FS_LIMIT = 0xFFFFFFFF,
  .GUEST_GS_LIMIT = 0xFFFFFFFF,
  .GUEST_CS_AR_BYTES = 0b1010000010011011,
  .GUEST_DS_AR_BYTES = 0b1100000010010011,
  .GUEST_SS_AR_BYTES = 0b1100000010010011,
  .GUEST_ES_AR_BYTES = 0b1100000010010011,
  .GUEST_FS_AR_BYTES = 0b1100000010010011,
  .GUEST_GS_AR_BYTES = 0b1100000010010011,
  .GUEST_LDTR_SELECTOR = 0,
  .GUEST_LDTR_BASE = 0,
  .GUEST_LDTR_LIMIT = 0,
  .GUEST_LDTR_AR_BYTES = 0x0082,
  /* Descriptor tables. */
  .HOST_GDTR_BASE = 0,                     /* filled in later */
  .GUEST_GDTR_BASE = 0,                    /* filled in later */
  .GUEST_GDTR_LIMIT = 0,                   /* filled in later */
  .HOST_IDTR_BASE = 0,                     /* filled in later */
  .GUEST_IDTR_BASE = 0,                    /* filled in later */
  .GUEST_IDTR_LIMIT = 0,                   /* filled in later */
  /* Task register -- a hack for now. */
  .HOST_TR_SELECTOR = 0x28,
  .GUEST_TR_SELECTOR = 0x28,
  .GUEST_TR_BASE = 0x6400,
  .GUEST_TR_LIMIT = 0x0067,
  .GUEST_TR_AR_BYTES = 0x008b,
  /* VM entry controls -- get started in long mode, etc. */
  /* !!! Minus the required bytes! !!! */

  .VM_ENTRY_CONTROLS = (1 << 9),
  .VM_EXIT_CONTROLS = (1 << 9) | (1 << 15),

  /* RFLAGS sanity. */
  .GUEST_RFLAGS = EFLAGS_BASE,
  .VMCS_LINK_POINTER = ~0UL
};

void hypv_entry(uint64_t vmxon_region) {
  uint64_t vmcsid;
  int status;

  /* Test CPUID. */
  status = vmx_support_test();
  if(status) {
    kprintf("[Hypv] Error on vmx_support_test. Halting.\n");
    goto error;
  }

  /* Make sure our VMX instructions aren't locked off. */
  status = vmx_lockout_test();
  switch(status) {
  case VMX_LOCKOUT_ERROR_LOCKED_OFF:
    /* Locked out of VMX by the BIOS. */
    kprintf("[Hypv] Error! BIOS locked the VMX Halting.\n");
    goto error;
  default:
    /* VMX locked on, good to go. */
    break;
  }

  /* Set the VMXE bit of Control Register 4 to enable VMX support. */
  asm volatile("movq %%cr4,%%rax    \n\t"
	       "bts $13,%%rax       \n\t"
	       "bts $7,%%rax       \n\t"
	       "movq %%rax,%%cr4    \n\t"
	       ::: "%rax");

  /* Test the control registers. */
  status = vmx_register_test();
  switch(status) {
  case VMX_REGISTER_ERROR_BAD_CR0:
    kprintf("[Hypv] %cr0 set incorrectly for VMX operation");
    goto error;
  case VMX_REGISTER_ERROR_BAD_CR4:
    kprintf("[Hypv] %cr4 set incorrectly for VMX operation");
    goto error;
  default:
    /* Registers are set correctly, good to go. */
    break;
  }

  /* Get the VMCS revision ID and set it in the VMXON region. */
  vmcsid = read_msr(IA32_VMX_BASIC);
  revid(vmcsid, vmxon_region);

  status = vmxon((uint64_t)virt2phys((void*)vmxon_region));
  switch(status) {
  case VM_STATUS_ERROR_INVALID:
    kprintf("[Hypv] VMXON returned VMfailInvalid\n");
    kprintf("    addr: %x\n"
	    "    rev: %x\n"
	    "    msr rev: %x\n",
	    vmxon_region, (uint32_t)vmcsid, *((uint32_t *)vmxon_region));
    goto error;
  case VM_STATUS_ERROR_VALID:
    kprintf("[Hypv] VMXON returned VMfailValid");
    goto error;
  default:
    /* VMXon was successful! */
    break;
  }

  return;
 error:
  panic();
}

/* Create a new VMCS and get it ready for launching. */
#define checkstatus if(status) goto error
#define write(field, value)			\
  status = vmwrite(field, value);		\
  checkstatus

void write_vmcs(vmcs_t *vmcs) {
  int status;

#ifdef DEBUG
  kprintf("	[Hypv] Initializing VMCS...\n");
  kprintf("	[Hypv] Host idtr_base = %x;\n", vmcs->HOST_IDTR_BASE);
  kprintf("	[Hypv] Guest idtr_base = %x; idtr_limit = %x\n", 
	  vmcs->GUEST_IDTR_BASE,
	  vmcs->GUEST_IDTR_LIMIT);
#endif

  /* Now initialize all the required regions. At this point, all required
   * feature checks have already been performed, so we don't need to do that
   * ourselves. */
  write(VIRTUAL_PROCESSOR_ID,      vmcs->VIRTUAL_PROCESSOR_ID);
  write(PIN_BASED_VM_EXEC_CONTROL, vmcs->PIN_BASED_VM_EXEC_CONTROL);
  write(CPU_BASED_VM_EXEC_CONTROL, vmcs->CPU_BASED_VM_EXEC_CONTROL);
  write(SECONDARY_VM_EXEC_CONTROL, vmcs->SECONDARY_VM_EXEC_CONTROL);
  write(EPT_POINTER,               vmcs->EPT_POINTER);
  write(EPT_POINTER_HIGH,          vmcs->EPT_POINTER_HIGH);
  write(EXCEPTION_BITMAP,          vmcs->EXCEPTION_BITMAP);
  write(CR0_GUEST_HOST_MASK,       vmcs->CR0_GUEST_HOST_MASK);
  write(CR4_GUEST_HOST_MASK,       vmcs->CR4_GUEST_HOST_MASK);
  /* -------- APIC Control Fields  -------- */
  write(APIC_ACCESS_ADDR,          vmcs->APIC_ACCESS_ADDR);
  write(APIC_ACCESS_ADDR_HIGH,     vmcs->APIC_ACCESS_ADDR_HIGH);
  /* -------- Host Processor State -------- */
  /* These shouldn't ever change from what we get after entering long mode. */
  write(HOST_CS_SELECTOR,          vmcs->HOST_CS_SELECTOR);
  write(HOST_DS_SELECTOR,          vmcs->HOST_DS_SELECTOR);
  write(HOST_SS_SELECTOR,          vmcs->HOST_SS_SELECTOR);
  write(HOST_ES_SELECTOR,          vmcs->HOST_ES_SELECTOR);
  write(HOST_FS_SELECTOR,          vmcs->HOST_FS_SELECTOR);
  write(HOST_GS_SELECTOR,          vmcs->HOST_GS_SELECTOR);
  write(HOST_TR_SELECTOR,          vmcs->HOST_TR_SELECTOR);
  write(HOST_CR0,                  vmcs->HOST_CR0);
  write(HOST_CR3,                  vmcs->HOST_CR3);
  write(HOST_CR4,                  vmcs->HOST_CR4);
  write(HOST_GDTR_BASE,            vmcs->HOST_GDTR_BASE);
  write(HOST_IDTR_BASE,            vmcs->HOST_IDTR_BASE);
  write(HOST_RIP,                  vmcs->HOST_RIP);
  vmcs->HOST_RSP = vcpu_ptr_array[this_cpu()]->stack;
  write(HOST_RSP,                  vmcs->HOST_RSP);
  /* -------- Guest processor state. -------- */
  /* For now, these should be the same as the host. */
  write(GUEST_CS_SELECTOR,         vmcs->HOST_CS_SELECTOR);
  write(GUEST_DS_SELECTOR,         vmcs->HOST_DS_SELECTOR);
  write(GUEST_SS_SELECTOR,         vmcs->HOST_SS_SELECTOR);
  write(GUEST_ES_SELECTOR,         vmcs->HOST_ES_SELECTOR);
  write(GUEST_FS_SELECTOR,         vmcs->HOST_FS_SELECTOR);
  write(GUEST_GS_SELECTOR,         vmcs->HOST_GS_SELECTOR);
  write(GUEST_TR_SELECTOR,         vmcs->GUEST_TR_SELECTOR);
  write(GUEST_CS_BASE,             vmcs->GUEST_CS_BASE);
  write(GUEST_DS_BASE,             vmcs->GUEST_DS_BASE);
  write(GUEST_SS_BASE,             vmcs->GUEST_SS_BASE);
  write(GUEST_ES_BASE,             vmcs->GUEST_ES_BASE);
  write(GUEST_FS_BASE,             vmcs->GUEST_FS_BASE);
  write(GUEST_GS_BASE,             vmcs->GUEST_GS_BASE);
  write(GUEST_CS_LIMIT,            vmcs->GUEST_CS_LIMIT);
  write(GUEST_DS_LIMIT,            vmcs->GUEST_DS_LIMIT);
  write(GUEST_SS_LIMIT,            vmcs->GUEST_SS_LIMIT);
  write(GUEST_ES_LIMIT,            vmcs->GUEST_ES_LIMIT);
  write(GUEST_FS_LIMIT,            vmcs->GUEST_FS_LIMIT);
  write(GUEST_GS_LIMIT,            vmcs->GUEST_GS_LIMIT);
  write(GUEST_CS_AR_BYTES,         vmcs->GUEST_CS_AR_BYTES);
  write(GUEST_DS_AR_BYTES,         vmcs->GUEST_DS_AR_BYTES);
  write(GUEST_SS_AR_BYTES,         vmcs->GUEST_SS_AR_BYTES);
  write(GUEST_ES_AR_BYTES,         vmcs->GUEST_ES_AR_BYTES);
  write(GUEST_FS_AR_BYTES,         vmcs->GUEST_FS_AR_BYTES);
  write(GUEST_GS_AR_BYTES,         vmcs->GUEST_GS_AR_BYTES);
  write(GUEST_LDTR_SELECTOR,       vmcs->GUEST_LDTR_SELECTOR);
  write(GUEST_LDTR_BASE,           vmcs->GUEST_LDTR_BASE);
  write(GUEST_LDTR_LIMIT,          vmcs->GUEST_LDTR_LIMIT);
  write(GUEST_LDTR_AR_BYTES,       vmcs->GUEST_LDTR_AR_BYTES);
  write(GUEST_GDTR_BASE,           vmcs->GUEST_GDTR_BASE);
  write(GUEST_GDTR_LIMIT,          vmcs->GUEST_GDTR_LIMIT);
  write(GUEST_IDTR_BASE,           vmcs->GUEST_IDTR_BASE);
  write(GUEST_IDTR_LIMIT,          vmcs->GUEST_IDTR_LIMIT);
  write(GUEST_TR_BASE,             vmcs->GUEST_TR_BASE);
  write(GUEST_TR_LIMIT,            vmcs->GUEST_TR_LIMIT);
  write(GUEST_TR_AR_BYTES,         vmcs->GUEST_TR_AR_BYTES);
  /* This gives the entry point to the guest! */
  write(GUEST_RIP,                 vmcs->GUEST_RIP);
  write(GUEST_RSP,                 vmcs->GUEST_RSP);
  /* Control registers, for now, identical to host. */
  write(GUEST_CR0,                 vmcs->GUEST_CR0);
  write(GUEST_CR3,                 vmcs->GUEST_CR3);
  write(GUEST_CR4,                 vmcs->GUEST_CR4);
  /* RFLAGS sanity check. */
  write(GUEST_RFLAGS,              vmcs->GUEST_RFLAGS);
  /* -------- VM entry/exit controls -------- */
  write(VM_ENTRY_CONTROLS,         vmcs->VM_ENTRY_CONTROLS);
  write(VM_EXIT_CONTROLS,          vmcs->VM_EXIT_CONTROLS);
  write(VMX_PREEMPTION_TIMER_VALUE,vmcs->VMX_PREEMPTION_TIMER_VALUE);
  /* -------- Miscellania -------- */
  write(VMCS_LINK_POINTER,         vmcs->VMCS_LINK_POINTER);
  
#ifdef DEBUG
  kprintf("	[Hypv] VMCS setup successful \n");
#endif
  return;
 error:
  kputs("[Hypv] error setting up vmcs\n");
  panic();
}
#undef checkstatus
#undef write

/* ****************************************************************************
 * Function:  uint32_t guest_not_interruptable()
 *
 * Description: Checks the guest state to see if it is ready to receive 
 * an interrupt
 *
 * MO:
 * At some point this will be necessary, currently not being used.
 * Once the hypervisor can specifically allow/disallow interrupts for the 
 * kernel, this will become relevant.
 *****************************************************************************/
uint32_t guest_interruptable() {
  uint64_t interruptable, rflags;		
  vmread(GUEST_INTERRUPTIBILITY_INFO, &interruptable);
  vmread(GUEST_RFLAGS, &rflags);

  /* rflags must be set to 1, and sti and mov_ss clear to 0    */
  /* so rflags * flags.if reduce to 1 (if it is interruptable) */
  /* interruptable reduces to 0 if it IS then negate           */
  /* finally flip the whole thing or just remove the first !   */
  return ( \
	  (rflags & RFLAGS_IF ) && \
	  !(interruptable & (GUEST_INTERRUPT_STATE_STI  |  GUEST_INTERRUPT_STATE_MOV_SS)) \
	   );	
  /* returns 0 for NOT interruptable */
  /* returns 1 if the guest is interruptable */
}

/****************************************************************************
 * Function: inject_event(vproc_t *vp, uint16_t vector, uint8_t type)
 *
 * Description: Modifies the entry control bits to inject an interrupt
 * Type Defs (bits 10:8): 0 Ext Int 1 Reserved 2 NMI 3 Hardware Excp 
 *	                  4 Software INT 5 Privileged software Excp 
 *			  6 Software Except 7 Other 
 * Bit 31 (valid) is cleared each vmexit and must be reset
 * Bit 11 Controls error code devliver 0 NO, 1 Yes
 ***************************************************************************/
void inject_event( uint16_t vector, uint8_t type){

  uint64_t value =  (1<< 31) | (0 << 11) | (type << 8) | vector;
  vmwrite(VM_ENTRY_INTR_INFO, value);

  return;
}

/*
 * Used to unset a bit in a particular execution field
 * Takes &variable to clear and the number position to set to zero
 */
void clear_bit(uint64_t * field, int position){
  (*field) &= ~(1 << position);
  return;
}

void hypv_map_bce_mmio(void *varg, void *vdev) {
  Pci_dev_t *dev;
  vproc_t *vp;

  dev = (Pci_dev_t *)vdev;
  vp = (vproc_t *)varg;

  
  if(is_supported_nic(dev)) {
    uint64_t mmio_pos;
    uint64_t mmio_end;
    uint64_t flash_pos;
    uint64_t flash_end;

    mmio_pos = dev->pci_dev_bars[0].bar_base;
    mmio_end = mmio_pos + dev->pci_dev_bars[0].bar_size;

    flash_pos = dev->pci_dev_bars[1].bar_base;
    flash_end = flash_pos + dev->pci_dev_bars[1].bar_size;

#ifdef DEBUG
    kprintf("	[Hypv] Network card mmio_pos = %x\n", mmio_pos);
    kprintf("	[Hypv] Network card mmio_end = %x\n", mmio_end);
#endif

    for( ; mmio_pos < mmio_end; mmio_pos += PAGE_SIZE)
      ept_map_page(vp->peptp, mmio_pos, mmio_pos, EPT_TYPE_UNCACHEABLE);

    for( ; flash_pos < flash_end; flash_pos += PAGE_SIZE)
      ept_map_page(vp->peptp, flash_pos, flash_pos, EPT_TYPE_UNCACHEABLE);

  }


  return;
}

void map_dma_to_guest(void *guest_vp) {
  uint64_t  guest_phys_dma, host_phys_dma, paddr;
  uint64_t target_guest; /* Where the guest wants it to be */
  uint64_t target_hypv;  /* Location we can access from hypv */
	
  vproc_t *vp = (vproc_t*)guest_vp;

  /*guest physical address it wants to use for DMA  in the second option RSI*/
  guest_phys_dma = vp->reg_storage.rsi;

  /*address where the guest expects to receive the actual address */
  target_guest = vp->reg_storage.rdx;
  
  /*host physical frame to write the repsonse in */
  paddr = ept_walk( target_guest, vp->peptp, 0, 0, 0 );
	
  /*host physical of DMA target */
  host_phys_dma = ept_walk(guest_phys_dma, vp->peptp, 0, 0, 0);

  /*need a virtual addr in the hypv so we can write the response 
    in w/o it faulting*/
  target_hypv = (uint64_t)vkmalloc( vk_heap, 1 );
  /*finally that has to be attached to the hypv's actual page tables */
  attach_page( target_hypv, paddr, PG_RW);

#ifdef NIC_DEBUG
  kprintf("guest physical = %x\n", guest_phys_dma);
  kprintf("target_guest = %x\n", target_guest);
  kprintf("target_hypv = %x\n", target_hypv);
  kprintf("real_phys_addr = %x\n", host_phys_dma);
#endif

  /*dereference the pointer in the hypv and write the address of the 
    actual dma target into that location */
  *(uint64_t*)(target_hypv | (target_guest & 0xfff) ) = host_phys_dma;

  /*free the virtual mapping from the hypervisor */
  vmem_free_temp( (uint64_t*)target_hypv, PAGE_SIZE );
  vkdirty( vk_heap, (void*)target_hypv, 1 );
  return;
}

static int is_supported_nic(Pci_dev_t *dev) {
  if (dev->vendor == PCI_VENDOR_INTEL &&
      (dev->dev_id == 0x100E || dev->dev_id ==0x107C || dev->dev_id ==0x10D3 || 
       dev->dev_id == 0x100F || dev->dev_id == 	0x153A)){
#ifdef DEBUG       
    kprintf("supported network card is %d, %d, %d \n", dev->bus, dev->slot, dev->func);   
    pci_print_caps(dev);
#endif    
    return 1;
  }
  return 0;
}
