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
#include <stdint.h>
#include <vmx.h>
#include <vmx_utils.h>
#include <interrupts.h>
#include <smp.h>
#include <semaphore.h>
#include <vmexit.h>
#include <apic.h>

int vmxon(uint64_t vmxon_region) {
  uint8_t invalid, valid;

  asm("vmxon %2\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid)
      : "m"(vmxon_region));

  VM_STATUS_RETURN(invalid, valid);
}

int vmclear(uint64_t vmcs) {
  uint8_t error;

  asm("vmclear %1\n\t"
      "setna %0"
      : "=r"(error)
      : "m"(vmcs));

  return error;
}

int vmlaunch() {
  uint8_t invalid, valid;

  asm("vmlaunch\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid));

  VM_STATUS_RETURN(invalid, valid);
}

int vmresume() {
  uint8_t invalid, valid;
  asm("vmresume\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid));

  VM_STATUS_RETURN(invalid, valid);
}

int vmptrld(uint64_t vmcs) {
  uint8_t invalid, valid;

  asm("vmptrld %2\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid)
      : "m"(vmcs));

  VM_STATUS_RETURN(invalid, valid);
}

int vmptrst(uint64_t *vmcs) {
  uint8_t invalid, valid;

  asm("vmptrst %2\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid)
      : "m"(*vmcs)
      : "memory");

  VM_STATUS_RETURN(invalid, valid);
}

int vmread(uint64_t field, uint64_t *value) {
  uint8_t invalid, valid;
  uint64_t fieldval;

  asm("vmread %3,%2\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid), "=r"(fieldval)
      : "r"(field));

  *value = fieldval;
  VM_STATUS_RETURN(invalid, valid);
}

int vmwrite(uint64_t field, uint64_t value) {
  uint8_t invalid, valid;

  asm("vmwrite %2,%3\n\t"
      VM_STATUS_CHECK
      : VM_STATUS_CONSTRAINTS(invalid, valid)
      : "rm"(value), "r"(field));

  VM_STATUS_RETURN(invalid, valid);
}

int vmx_support_test() {
  uint64_t features;
  uint64_t mask;

  features = cpuid(1, CPUID_ECX);
  mask = features & CPUID_FEATURE_VMX;
#ifdef DEBUG
  kprintf("[HYPV] core %s needed VMX features\n",
	  mask ? "supports" : "does not support");
#endif
  if(!mask) {
    /* Right now, the only failure mode is missing a specific bit.             
     * To get all the missing bits if this changes in the future, do:          
     *     mask ^= CPUID_FEATURE_VMX
     * The missing bits in CPUID %ecx will then be the set bits in mask.       
     */
    return VMX_SUPPORT_ERROR_NO_VMX;
  }

  return VMX_SUPPORT_SUCCESS;
}

int vmx_lockout_test() {
  uint64_t msr;

  msr = read_msr(IA32_FEATURE_CONTROL);
  if(!(msr & (1 << 2))) {                  /* VMXON-allowed bit */
    if(msr & 1) {                        /* Lock bit */
      /* VMXON is locked to "off" (bit set) by the BIOS. Can't run VMX. */
      return VMX_LOCKOUT_ERROR_LOCKED_OFF;
    } else {
      /* VMX lock bit is clear, which means we're okay. Set it,                
       * as well as the VMXON-allowed bit (why do they have so                 
       * many necessary bits? */
      msr |= (1 | (1 << 2));
      write_msr(IA32_FEATURE_CONTROL, msr);
    }
  } else {
    /* VMXON is allowed by BIOS, now the lock bit must also be turned          
     * on if it isn't already. */
    if(!(msr & 1)) {
      /* Set the VMXON lock bit. */
      msr |= 1;
      write_msr(IA32_FEATURE_CONTROL, msr);
    }
  }
  return VMX_LOCKOUT_SUCCESS;
}

/* Tests to make sure VMXON will go okay when issued. */
int vmx_register_test() {
  uint64_t cr0, cr0fixed, cr4, cr4fixed;

#ifdef DEBUG
  kprintf("     [Hypv] Testing control registers...\n");
#endif
  /* Move the current control registers and their fixed forms into our the    
   * variables specified for them, for error reporting purposes. */
  cr0 = read_cr0();
  cr4 = read_cr4();
  cr0fixed = read_msr(IA32_VMX_CR0_FIXED0);
  cr4fixed = read_msr(IA32_VMX_CR4_FIXED0);
  cr0 = (cr0 & cr0fixed) ^ cr0fixed;
  cr4 = (cr4 & cr4fixed) ^ cr4fixed;
  /* If any of these tests fail, the missing bits for that control            
   * register can be found in the values of the corresponding variable         
   * (i.e., if cr0 fails the missing bits will be found in variable cr0.       
   */
  if(cr0)
    return VMX_REGISTER_ERROR_BAD_CR0;
  if(cr4)
    return VMX_REGISTER_ERROR_BAD_CR4;

#ifdef DEBUG
  kprintf("     [Hypv] Control registers Set\n");
#endif
  return VMX_REGISTER_SUCCESS;
}

void add_to_memmap(vproc_t *vp, uint64_t base, uint64_t length, int type) {

  int i, j;
  uint64_t off = 0x0;

  /* find where in the memmap we should put this chunk */
  for ( i = 0; i < *vp->memmap_entries; i++ ) {
    if ( base <= off )
      break;
    off += vp->memmap[i].length;
  }

  /* copy any entries that should follow the new entry over by one */
  for ( j = *vp->memmap_entries; j > i; j-- )
    vp->memmap[j] = vp->memmap[j+1];

  /* set the new memmap entry */
  vp->memmap[i].base = base;
  vp->memmap[i].length = length;
  vp->memmap[i].type = type;

  /* increment the number of memmap entries */
  *vp->memmap_entries = *vp->memmap_entries + 1;

  return;
}

/****************************************************************************
 * Function: static void print_gpregs(vproc_t *vp)
 *
 * Description: Prints out the current state of the general purpose registers
 ****************************************************************************/ 
void print_gpregs(vproc_t *vp){

  uint64_t idt, RSP, RIP, RFLAGS;
  uint64_t intr_info;
  uint64_t entry_error_code;
  uint64_t cs_sel;
  uint64_t ss_sel;
	

  vmread(GUEST_IDTR_BASE, & idt);
  vmread(VM_ENTRY_INTR_INFO, &intr_info);
  vmread(VM_ENTRY_EXCEPTION_ERROR_CODE, &entry_error_code);
  vmread(GUEST_RIP, & RIP);
  vmread(GUEST_RSP, & RSP);
  vmread(GUEST_RFLAGS, & RFLAGS);
  vmread(GUEST_CS_SELECTOR, &cs_sel);
  vmread(GUEST_SS_SELECTOR, &ss_sel);

  kprintf("AX %x  ", vp->reg_storage.rax);	
  kprintf("BX %x  ", vp->reg_storage.rbx);	
  kprintf("CX %x\n", vp->reg_storage.rcx);
  kprintf("DX %x  ", vp->reg_storage.rdx);
  kprintf("RSI %x ", vp->reg_storage.rsi);
  kprintf("RDI %x\n", vp->reg_storage.rdi);
  kprintf("R8 %x ", vp->reg_storage.r8);
  kprintf("R9 %x ", vp->reg_storage.r9);
  kprintf("R10 %x ", vp->reg_storage.r10);
  kprintf("R11 %x ", vp->reg_storage.r11);
  kprintf("R12 %x ", vp->reg_storage.r12);
  kprintf("R13 %x ", vp->reg_storage.r13);
  kprintf("R14 %d ", vp->reg_storage.r14);
  kprintf("R15 %d \n", vp->reg_storage.r15);
	
  kprintf("CS  %x  ", cs_sel);
  kprintf("SS  %x \n", ss_sel);	
	
  kprintf("RIP %x\n", RIP);
  kprintf("RSP %x\n", RSP);
  kprintf("RBP %x\n", vp->reg_storage.rbp);	
  kprintf("IDT vec %x  ", idt);
  kprintf("FLAGS %x\n", RFLAGS);	
		
  return;
}

#ifndef KERNEL
/* Add the run-time memory info, and the bits that must be set and clear, 
 * to the vmcs default structures. 
 */
void init_vmcs_defaults() {
  struct gdt_desc desc;
  uint64_t msr;

  /* Set up the MSRs. */
  vmx_msrs.MSR_IA32_FEATURE_CONTROL = read_msr(IA32_FEATURE_CONTROL);
  vmx_msrs.MSR_IA32_VMX_BASIC = read_msr(IA32_VMX_BASIC);
  vmx_msrs.MSR_IA32_VMX_PINBASED_CTLS = read_msr(IA32_VMX_PINBASED_CTLS);
  vmx_msrs.MSR_IA32_VMX_PROCBASED_CTLS = read_msr(IA32_VMX_PROCBASED_CTLS);
  vmx_msrs.MSR_IA32_VMX_EXIT_CTLS = read_msr(IA32_VMX_EXIT_CTLS);
  vmx_msrs.MSR_IA32_VMX_MISC = read_msr(IA32_VMX_MISC);
  vmx_msrs.MSR_IA32_VMX_CR0_FIXED0 = read_msr(IA32_VMX_CR0_FIXED0);
  vmx_msrs.MSR_IA32_VMX_CR0_FIXED1 = read_msr(IA32_VMX_CR0_FIXED1);
  vmx_msrs.MSR_IA32_VMX_CR4_FIXED0 = read_msr(IA32_VMX_CR4_FIXED0);
  vmx_msrs.MSR_IA32_VMX_CR4_FIXED1 = read_msr(IA32_VMX_CR4_FIXED1);
  vmx_msrs.MSR_IA32_VMX_PROCBASED_CTLS2 = read_msr(IA32_VMX_PROCBASED_CTLS2);
  vmx_msrs.MSR_IA32_VMX_TRUE_PINBASED_CTLS = read_msr(IA32_VMX_TRUE_PINBASED_CTLS);
  vmx_msrs.MSR_IA32_VMX_TRUE_PROCBASED_CTLS = read_msr(IA32_VMX_TRUE_PROCBASED_CTLS);
  vmx_msrs.MSR_IA32_VMX_TRUE_EXIT_CTLS = read_msr(IA32_VMX_TRUE_EXIT_CTLS);
  vmx_msrs.MSR_IA32_VMX_TRUE_ENTRY_CTLS = read_msr(IA32_VMX_TRUE_ENTRY_CTLS);

  if(!(vmx_msrs.MSR_IA32_VMX_BASIC & (1UL << 55))) {
    kputs("Error: Cannot use TRUE controls.");
    panic();
  }

  /* Set up the run-time defaults and double-check them. */
  /* For pin-based, these bits need to be cleared. */
  msr = vmx_msrs.MSR_IA32_VMX_PINBASED_CTLS;
#ifdef DEBUG
  kprintf("[Hypv] VMX pin based controls support %x \n", msr);
#endif
  if(((msr >> 32) & vmcs_default.PIN_BASED_VM_EXEC_CONTROL) ^
     vmcs_default.PIN_BASED_VM_EXEC_CONTROL)
    goto error;
  else
    vmcs_default.PIN_BASED_VM_EXEC_CONTROL |= (uint32_t)msr;

  /* For proc-based, these bits need to be cleared. */
  msr = vmx_msrs.MSR_IA32_VMX_TRUE_PROCBASED_CTLS;
#ifdef DEBUG
  kprintf("[Hypv] VMX primary exe process controls support %x \n", msr);
#endif
  if(((msr >> 32) & vmcs_default.CPU_BASED_VM_EXEC_CONTROL) ^
     vmcs_default.CPU_BASED_VM_EXEC_CONTROL)
    goto error;
  else
    vmcs_default.CPU_BASED_VM_EXEC_CONTROL |= (uint32_t)msr;

  /* Secondary proc-based. */
  msr = vmx_msrs.MSR_IA32_VMX_PROCBASED_CTLS2;
#ifdef DEBUG
  kprintf("[Hypv] VMX secondary exe process controls support %x \n", msr>>32);
#endif
  if(((msr >> 32) & vmcs_default.SECONDARY_VM_EXEC_CONTROL) ^
     vmcs_default.SECONDARY_VM_EXEC_CONTROL)
    goto error;
  else
    vmcs_default.SECONDARY_VM_EXEC_CONTROL |= (uint32_t)msr;

  msr = vmx_msrs.MSR_IA32_VMX_TRUE_ENTRY_CTLS;
  if(((msr >> 32) & vmcs_default.VM_ENTRY_CONTROLS) ^
     vmcs_default.VM_ENTRY_CONTROLS)
    goto error;
  else
    vmcs_default.VM_ENTRY_CONTROLS = (uint32_t)msr |
      vmcs_default.VM_ENTRY_CONTROLS;

  msr = vmx_msrs.MSR_IA32_VMX_TRUE_EXIT_CTLS;
  if(((msr >> 32) & vmcs_default.VM_EXIT_CONTROLS) ^
     vmcs_default.VM_EXIT_CONTROLS)
    goto error;
  else
    vmcs_default.VM_EXIT_CONTROLS = (uint32_t)msr |
      vmcs_default.VM_EXIT_CONTROLS;

  /* CR0 */
  msr = read_cr0();
  vmcs_default.HOST_CR0 = msr;
  vmcs_default.GUEST_CR0 = msr;
  /* CR3 */
  msr = read_cr3();                        /* Host only */
  vmcs_default.HOST_CR3 = msr;
  /* CR4. There's a number of bits that _have_ to be zero or one here, that we
   * don't otherwise use (like the virtual interrupt flag). */
  msr = read_cr4();
  vmcs_default.HOST_CR4 |= msr;
  msr |= read_msr(IA32_VMX_CR4_FIXED0);    /* Bits that must be 1 */
  msr &= read_msr(IA32_VMX_CR4_FIXED1);    /* Bits that must be 0 */
  vmcs_default.GUEST_CR4 = msr;

  asm volatile("sgdt %0" : "=m"(desc));
  vmcs_default.HOST_GDTR_BASE = desc.base;
  vmcs_default.GUEST_GDTR_BASE = desc.base;
  vmcs_default.GUEST_GDTR_LIMIT = desc.limit;

  asm volatile("sidt %0" : "=m"(desc));
  vmcs_default.HOST_IDTR_BASE = desc.base;
  vmcs_default.GUEST_IDTR_BASE = desc.base;
  vmcs_default.GUEST_IDTR_LIMIT = desc.limit;

  return;
 error:
  kputs("error setting up defaults. Halting.");
  panic();
}
#endif

/*This is a c rapper for use in asm.S for hypervisor transitions. It acquires*/
/*the hypervisor lock                                                        */
void hypv_acquire_lock(){
  acquire_lock(sem_hypv);
  return;
}

/*This is a c rapper for use in asm.S for hypervisor transitions. It releases*/
/*the hypervisor lock                                                        */
void hypv_release_lock(){
  release_lock(sem_hypv);
  return;
}

/*This function returns the vcpu_t pointer releated to the core it is running*/
/*on. This is similar to the ksched_get_last. Its primary purpose is for     */
/*the asm.S file the hypervisor uses in transition from guest to hypervisor  */
/*and back again. In that case the pointer is returned in rax                */
vcpu_t *vcpu_get_last(){
  return *(vcpu_ptr_array + this_cpu());
}
