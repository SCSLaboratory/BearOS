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
#include <kstdio.h>
#include <mcontext.h>
#include <memory.h>
#include <stdint.h>
#include "asm.h"
#include <vmx.h>
#include <apic.h>
#include <ept.h>
#include <kvmem.h>
#include <vmexit.h>
#include <vmx_utils.h>
#include <smp.h>
#include <vmexit.h>
#include <vmx_utils.h>
#include <vproc.h>

//#define APIC_DEBUG 1 /*Debug flag for APIC in this file */
/*This is for joining cores to another guest. The  Intel startup algorithem */
/*uses three ipis. We count the three ipis and then join the other core to  */
/*the guest.                                                                */
int count_startup_ipis = 0;

uint64_t time;

const char *vmx_ctrl_reg_access_name_list[] = {
  "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI", "R8", "R9", "R10",
  "R11", "R12", "R13", "R14", "R15",
};

const char *vm_exit_reasons[] = {
  /* 00 */ "Exception or non-maskable interrupt (NMI).",
  /* 01 */ "External interrupt.",
  /* 02 */ "Triple fault.",
  /* 03 */ "INIT signal.",
  /* 04 */ "Start-up IPI (SIPI).",
  /* 05 */ "I/O system-management interrupt (SMI).",
  /* 06 */ "Other SMI.",
  /* 07 */ "Interrupt window.",
  /* 08 */ "NMI window.",
  /* 09 */ "Task switch.",
  /* 10 */ "CPUID.",
  /* 11 */ "GETSEC.",
  /* 12 */ "HLT.",
  /* 13 */ "INVD.",
  /* 14 */ "INVLPG.",
  /* 15 */ "RDPMC.",
  /* 16 */ "RDTSC.",
  /* 17 */ "RSM.",
  /* 18 */ "VMCALL.",
  /* 19 */ "VMCLEAR.",
  /* 20 */ "VMLAUNCH.",
  /* 21 */ "VMPTRLD.",
  /* 22 */ "VMPTRST.",
  /* 23 */ "VMREAD.",
  /* 24 */ "VMRESUME.",
  /* 25 */ "VMWRITE.",
  /* 26 */ "VMXOFF.",
  /* 27 */ "VMXON.",
  /* 28 */ "Control-register accesses.",
  /* 29 */ "MOV DR.",
  /* 30 */ "I/O instruction.",
  /* 31 */ "RDMSR.",
  /* 32 */ "WRMSR.",
  /* 33 */ "VM-entry failure due to invalid guest state.",
  /* 34 */ "VM-entry failure due to MSR loading.",
  /* 35 */ "reserved",
  /* 36 */ "MWAIT.",
  /* 37 */ "Monitor trap flag.",
  /* 38 */ "reserved",
  /* 39 */ "MONITOR.",
  /* 40 */ "PAUSE.",
  /* 41 */ "VM-entry failure due to machine check.",
  /* 42 */ "reserved",
  /* 43 */ "TPR below threshold.",
  /* 44 */ "APIC access.",
  /* 45 */ "reserved",
  /* 46 */ "Access to GDTR or IDTR.",
  /* 47 */ "Access to LDTR or TR.",
  /* 48 */ "EPT violation.",
  /* 49 */ "EPT misconfiguration.",
  /* 50 */ "INVEPT.",
  /* 51 */ "RDTSCP.",
  /* 52 */ "VMX-preemption timer expired.",
  /* 53 */ "INVVPID.",
  /* 54 */ "WBINVD.",
  /* 55 */ "XSETBV.  ",
};

static void restore_gpregs(vproc_t *vp) {

  kmemcpy(vcpu_ptr_array[this_cpu()], &(vp->reg_storage), 
	  sizeof(struct mcontext) - sizeof(char *));
  
  kmemcpy(vcpu_ptr_array[this_cpu()]->reg_storage.sse,vp->reg_storage.sse,512);

  return;
}

static void save_gpregs(vproc_t *vp) {

  kmemcpy(&(vp->reg_storage), vcpu_ptr_array[this_cpu()], 
	  sizeof(struct mcontext) - sizeof(char *));

  kmemcpy(vp->reg_storage.sse,vcpu_ptr_array[this_cpu()]->reg_storage.sse,512);

  return;
}

void start_vp(){
  vproc_t* vp;
  uint64_t vpid;
  int rc;

  acquire_lock(sem_hypv);

  kprintf("[%d] hypv lock\n", this_cpu());

  lapic_eoi();

  /*This does some vmx support tests and calls vmxon */
  hypv_entry(vcpu_ptr_array[this_cpu()]->vmxon_region_virt);

  kprintf("this %d got start ipi\n");

  if( this_cpu() == 0 )
    vpid = 1;
  else
    vpid = 2;

  vp = vproc_get(vpid);
  
  kprintf("launching vproc with id %d",vp->vproc_id);
  restore_gpregs(vp);
  rc =  launch_vproc(vp);
  
  kprintf("error; return code %d.\n", rc);
  kputs("Halting now.");
  asm volatile("hlt");
  
  return;
}


static void exception_nmi_handler( uint64_t int_info, 
				   uint64_t qualification,
				   uint64_t vectoring_info ) {   

  uint64_t interrupt;
  uint8_t vec, type;
 
  if (!(int_info & 0x80000000)) /* check bit 31 valid? */
    kprintf("state wasn't valid");
  //this check is important because it tells you the architectural state
  //is actually valid, otherwise what you have is 1 instruction before the 
  //event which caused the exit.

			
  /* lower 7 bits store the vector of exception,
   * NMI =2, or int number */
  vec = int_info & 0xFF;
  interrupt = vectoring_info & 0xFF;
  /* bits 8-10 tell us the type, then shift over */
  type = (int_info & 0x700) >> 8;

  /* Meaningful comment needed here why 0xb? */
  if(vec == 0xb){

    switch(interrupt){
      case 0x0E: /* Page fault */
	kprintf ("Page Fault at %x\n", qualification);
	break;

      case 0x20: /* Timer interrupt */
	break;
    }
  }

  return;
}

static void external_interrupt_handler( vproc_t *old_vp, uint64_t int_info ) {

  uint64_t exe_control_bits;
  uint8_t vec;
  waiting_interrupt_t  pending_int;

  vec = int_info & 0xFF;
		
  if(vec == 0x20){
    /*fixme this will need to support multiple vm's one day */
    if(guest_interruptable())   
      inject_event( 0x20, 0);
  }
  if(vec == 0x21){
    if(guest_interruptable()){
      inject_event( 0x21, 0);
    }
    else{
      /*save the vector */
      pending_int.vector = vec; 
      vector_push(&old_vp->pending_interrupts_vec, &pending_int);
      /*enable interrupt window exiting */
      vmread( CPU_BASED_VM_EXEC_CONTROL, &exe_control_bits);
      exe_control_bits = exe_control_bits ^ (1<<2);
      vmwrite(CPU_BASED_VM_EXEC_CONTROL, exe_control_bits);
    }
  }
  if(vec == 0x2b)
    kprintf("unhandled hypervisor external interrupt %x \n", vec);
				
				
  lapic_eoi();
  restore_gpregs(old_vp);
  launch_vproc(old_vp);


  return;
}

static void interrupt_window_exiting_handler( vproc_t *old_vp ) {

  waiting_interrupt_t  pending_int;
  uint64_t exe_control_bits;

  /*first check to see if there's any work to do */
  /* hypothetically we don't need this but just in case */
  if( vector_size(&old_vp->pending_interrupts_vec)/sizeof(waiting_interrupt_t))  {
    /*test again if it's interruptable and if not bail back leaving 
     *window exiting on
     */
    if(guest_interruptable()){
			
      vector_pop(&old_vp->pending_interrupts_vec, &pending_int);
      kprintf("itnerrupting! %d \n ", pending_int.vector);
      inject_event(pending_int.vector, 0); 		
    }
    else{
      kprintf("NOT interrupting \n  ");
      restore_gpregs(old_vp);
      launch_vproc(old_vp);	
    }		
  }	
  /*if the vector isn't empty leave window exiting on until the next exit*/
  if( vector_size(&old_vp->pending_interrupts_vec)/sizeof(waiting_interrupt_t)){
    kprintf(" leaving window exiting on\n  ");
    restore_gpregs(old_vp);
    launch_vproc(old_vp);
  }
  else{
    kprintf("window off \n  ");
    vmread( CPU_BASED_VM_EXEC_CONTROL, &exe_control_bits);
    clear_bit(&exe_control_bits, 2);
    vmwrite(CPU_BASED_VM_EXEC_CONTROL, exe_control_bits);
    restore_gpregs(old_vp);
    launch_vproc(old_vp);
  }	

  return;
}

static void cpuid_handler( vproc_t *vp ){
  
  uint64_t ins_len, RIP;

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a cpuid */  
  vmread(GUEST_RIP, &RIP);
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmwrite(GUEST_RIP, RIP+ins_len);

  kprintf("cpuid handler\nfeatures to be checked = 0x%x\n", vp->reg_storage.rax);
 
  /* cpuid returns values in both ECX and EDX. Rather than figure out
     what bear wanted with its cpuid wrapper function we will just 
     return both by modifying the guest registers here. */
  vp->reg_storage.rcx = cpuid( vp->reg_storage.rax, CPUID_ECX );
  vp->reg_storage.rdx = cpuid( vp->reg_storage.rax, CPUID_EDX );

  /* relaunch the guest */
  restore_gpregs(vp);
  launch_vproc(vp);

  return;
}

/*This should be moved to vproc.c, but gpreg stuff is static in here (RMD)...*/
static void switch_vproc(int vpid, vproc_t *old_vp){
  vproc_t *vp_next; 
  int rc;
  if( (vpid >= get_vpid_cnt() ) || vpid == 0)
    { 
      kprintf("WARNING attempt to run non existant VP \n");
      kprintf("Attempting to relaunch last vpid\n");
      restore_gpregs(old_vp);
      rc = launch_vproc(old_vp); 
      goto error;   
    }
  
  vp_next = vproc_get(vpid);
  if(vp_next != NULL)
    {       
      kprintf("launching vproc with id %d",vp_next->vproc_id);
      restore_gpregs(vp_next);
      rc =  launch_vproc(vp_next);
      goto error;
    }
  
 error :
  kprintf("error; return code %d.\n", rc);
  kputs("Halting now.");
  asm volatile("hlt");  
}   

static void vmcall_handler( vproc_t *vp ){
  
  uint64_t ins_len, RIP, vmcall_option;

  /*
   * These have to be done to increment the guest RIP
   * or it will just sit in an endless loop
   */	
  vmread(GUEST_RIP, & RIP);
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmwrite(GUEST_RIP, RIP+ins_len );
		
  /*By convention we've been putting the option in the first reg/argument */ 
  vmcall_option = vp->reg_storage.rdi;

  if(vmcall_option == 6){
    map_dma_to_guest(vp);
  }

#ifdef XONLY_BEAR_HYPV
  if(vmcall_option == 9){
      mark_kernel_execute_only(vp);
  }
#endif

#ifdef DEBUG  
  if(vmcall_option == 35){
    kprintf("hypervisor memory vmcall\n");
    print_gpregs(vp);
  }
#endif
  if(vmcall_option == 40){
    kprintf("request to enable interrupt %d \n", vp->reg_storage.rsi);
    ioapicenable(	vp->reg_storage.rsi, 0);
  }
  restore_gpregs(vp);
  launch_vproc(vp);
     
  return;
}

static void vmclear_handler( vproc_t* vp ){
  uint64_t ins_len, vp_RIP, rflags, vaddr;


  kprintf("entered VMCLEAR handler\n");
  
  /* Read in the guest flags for later use */
  vmread(GUEST_RFLAGS, &rflags);

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a vmclear */
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);  

  /* case vmclear executed by guest time for nesting                       */
  /* save the guest physical vmclear region used by the guest              */
  /* also save the host physical address that is associated with the guest */
  /* physical address for any other acounting needs later                  */
  vp->vmcs_shadow.guest_vmcs_ptr_phys = vp->reg_storage.rdi;
  vp->vmcs_shadow.host_vmcs_ptr_phys = ept_walk(vp->reg_storage.rdi, vp->peptp, NULL, NULL, NULL);
  kprintf("guest phsycial vmclear region = 0x%x\n", vp->vmcs_shadow.guest_vmcs_ptr_phys);
  kprintf("host physcial vmclear region = 0x%x\n", vp->vmcs_shadow.host_vmcs_ptr_phys);

  /* I am not sure if this is necessary for the host to maintain a virtual
     mapping to the secondary vmcs. (most likely not), but for now I am 
     creating one to alleviate future accounting problems I may run into */
  vaddr = vkmalloc(vk_heap, 1);
  attach_page(vaddr, vp->vmcs_shadow.host_vmcs_ptr_phys, PG_RW);
  vp->vmcs_shadow.host_vmcs_ptr = (void*)vaddr;

  kprintf("vp->vmcs_shadow.host_vmcs_ptr = 0x%x\n", *(uint64_t*)vp->vmcs_shadow.host_vmcs_ptr);

  if( vmclear(vp->vmcs_shadow.host_vmcs_ptr_phys) ){
    kprintf("[VMEXIT] error setting up nested vmcs vmclear\n");
    panic();
  }
  /* We succeeded and need to make sure our rflags are in order */
  /* First read in the guest rflags */
  kprintf("rflags = 0x%x\n", rflags);
  /* Then zero out the zero flag in bit 6 and the carry flag in bit 0 */
  rflags &= ~(1 << 6);
  rflags &= ~1;
  kprintf("rflags = 0x%x\n", rflags);
  vp->reg_storage.eflags = rflags;
  /* Write the new rflags back to the guest */
  vmwrite(GUEST_RFLAGS, rflags);

  /* relaunch the guest */
  restore_gpregs(vp);
  launch_vproc(vp);
  
  return;
}

static void vmlaunch_handler( vproc_t* vp ){
  uint64_t guest_peptp;
  uint64_t vmcs_shadow_host_guest_peptp;
  uint64_t status;

  uint64_t exit_reason, gcr3, grip, qualification, address;


  kprintf("entered VMLAUNCH handler\n");
  
  if( vmptrld(vp->vmcs_shadow.host_vmcs_ptr_phys) ){
    kprintf("[VMEXIT] error setting up nested vmcs vmptrload\n");
    panic();
  }

  vmread(EPT_POINTER,&guest_peptp);
  
  vmcs_shadow_host_guest_peptp = ept_walk(guest_peptp, vp->peptp, NULL, NULL, NULL);

  kprintf("guest_peptp = 0x%x\n", guest_peptp);
  kprintf("vmcs_shadow_host_guest_peptp = 0x%x\n", vmcs_shadow_host_guest_peptp);

  vmcs_shadow_host_guest_peptp |= (3 << 3);
  vmcs_shadow_host_guest_peptp |= 6;

  vmwrite(EPT_POINTER, vmcs_shadow_host_guest_peptp);
  vmwrite(EPT_POINTER_HIGH, (vmcs_shadow_host_guest_peptp >> 32));

  kprintf("enter the lulz\n");

  vmlaunch();

  kprintf("epic failure\n");

  vmread(VM_INSTRUCTION_ERROR, &status);

#ifdef DEBUG_SHIM
  kprintf("what field information did we screw up %d\n", status);
#endif
  
  vmread(VM_EXIT_REASON, &exit_reason);

  if(exit_reason & (1 << 31)){
    kputs("vm entry failure");
    exit_reason &= ~(1 << 31);
  }

  kprintf("exit reason = %d\n", exit_reason);

  vmread(VM_INSTRUCTION_ERROR, &status);
  kprintf("error field = %d\n", status);

  vmread(GUEST_CR3, &gcr3);
  kprintf("offending cr3 0x%x\n", gcr3);

  vmread(GUEST_RIP, &grip);
  kprintf("guest rip that died 0x%x\n", grip);

  vmread(EXIT_QUALIFICATION, &qualification);

  if(qualification & 0x1)
    kprintf("violation was EPT read\n");
  if(qualification & 0x2)
    kprintf("violation was EPT write\n");
  if(qualification & 0x4)
    kprintf("violation was EPT instruction fetch\n");
  if(qualification & 0x80)
    vmread(GUEST_LINEAR_ADDRESS, &address);

  kprintf("Linear Address access violation at %X\n", address);

  vmread(GUEST_PHYSICAL_ADDRESS, &address);
  
  kprintf("Physical Address access violation at %x\n", address);

  return;
}


static void vmptrload_handler( vproc_t* vp ){
  uint64_t ins_len, vp_RIP, rflags;

  kprintf("entered vmptrload_handler\n");

  /* read in the guest flags for later use */
  vmread(GUEST_RFLAGS, &rflags);

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a vmptrld */
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);  

  /* !!!! This has now changed what the processor is referencing in terms of */
  /* vmreads and vmwrites. This has happened because we are now pointing at  */
  /* the vmcs structure created by the guest. Thus, after this we must switch*/
  /* back to the vmcs structure that is the guest that is running directly   */
  /* above this hypervisors context. I.E. level 0 should point to level 1 and*/
  /* level 1 should point to level 2, NOT level 0 pointing to level 2 TURTLES*/
  if( vmptrld(vp->vmcs_shadow.host_vmcs_ptr_phys) ){
    kprintf("[VMEXIT] error setting up nested vmcs vmptrload\n");
    panic();
  }
  if( vmptrld(vp->vmcs_ptr_phys) ){
    kprintf("[VMEXIT] error setting up nested vmcs vmptrload\n");
    panic();
  }
  /* We succeeded at both ptrloads, now make sure our rflags are correct*/
  /* Then zero out the zero flag in bit 6 and the carry flag in bit 0 */
  rflags &= ~(1 << 6);
  rflags &= ~1;
  vp->reg_storage.eflags = rflags;
  /* Write the new rflags back to the guest */
  vmwrite(GUEST_RFLAGS, rflags);    

  /* relaunch the guest */
  restore_gpregs(vp);
  launch_vproc(vp);

  return;
}

static void vmwrite_handler( vproc_t* vp ){
  uint64_t ins_len, vp_RIP, rflags;

  vmread(GUEST_RFLAGS, &rflags);

  kprintf("entered vmwrite handler\n");

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a vmptrld */
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);  


  /* The VMWRITE handler is a special case as the context must switch to the */
  /* context of what the level 1 guest is writing to its level 2 guest. The  */
  /* level 0 guest will then write the information pertaining to the level 2 */
  /* guest. Then the context must switch back to that of the level 1 guest   */
  /* for the level 0 hypervisor to report success back to the level 1 guest  */

  /* switch to writing to level 2 guest vmcs */
  if( vmptrld(vp->vmcs_shadow.host_vmcs_ptr_phys) ){
      kprintf("[VMEXIT] error setting up nested vmcs vmptrload\n");
      panic();
  }

  vmwrite(vp->reg_storage.rdi, vp->reg_storage.rsi);

  kprintf("VMWRITE handler field = 0x%x\n", vp->reg_storage.rdi);
  kprintf("VMWRITE handler value = 0x%x\n", vp->reg_storage.rsi);

  /* switch back to writing to the level 1 guest vmcs */
  if( vmptrld(vp->vmcs_ptr_phys) ){
    kprintf("[VMEXIT] error setting up nested vmcs vmptrload\n");
    panic();
  }

  /* We succeeded at both ptrloads and the vmwrite.                   */
  /* now make sure our rflags are correct                             */
  /* Then zero out the zero flag in bit 6 and the carry flag in bit 0 */
  rflags &= ~(1 << 6);
  rflags &= ~1;
  vp->reg_storage.eflags = rflags;
  /* Write the new rflags back to the guest */
  vmwrite(GUEST_RFLAGS, rflags);

  /* relaunch the guest */
  restore_gpregs(vp);
  launch_vproc(vp);  

  return;
}

static void vmxon_handler( vproc_t* vp ){
  uint64_t ins_len, vp_RIP, rflags, vaddr;

  kprintf("entered VMXON handler\n");

  /* Read in the guest flags for later use */
  vmread(GUEST_RFLAGS, &rflags);
  
  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a vmxon */
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);

  /* case vmxon executed by guest time for nesting                         */
  /* save the guest physical vmxon region used by the guest                */
  /* also save the host physical address that is associated with the guest */
  /* physical address for any other acounting needs later                  */
  vp->vmcs_shadow.guest_vmxon_region_phys = vp->reg_storage.rdi;
  vp->vmcs_shadow.host_vmxon_region_phys = ept_walk(vp->reg_storage.rdi, vp->peptp, NULL, NULL, NULL);
  kprintf("guest phsycial vmxon region = 0x%x\n", vp->vmcs_shadow.guest_vmxon_region_phys);
  kprintf("host physcial vmxon region = 0x%x\n", vp->vmcs_shadow.host_vmxon_region_phys);

  /* no virtual mapping exists between the guest and the hypervisor        */
  /* Thus to access the guest revesion ID, a temporary mapping is created  */
  /* to the physical address the guest's vmxon_region is located at        */
  /* then as the revid is loaded into the lower 16 bytes (4k range) of the */
  /* the guest's vmxon region we can mask off them to obtain the revid     */
  vaddr = vkmalloc(vk_heap, 1);
  attach_page(vaddr, vp->vmcs_shadow.host_vmxon_region_phys, PG_RW);

  vp->vmcs_shadow.vm_revid = (*(uint64_t*)vaddr & 0xFFFF);
  kprintf("guest_revid = 0x%x\n", vp->vmcs_shadow.vm_revid);

  /* It is not possible to call vmxon again. Thankfully the guest does not */
  /* know that it is nested and we are just skipping the call all together */
  /* however, the rflags need to be set accordingly so the guest believes  */
  /* the call actually succeeded (note may be special to bear)             */
  
  /* In RFLAGS, zero out the zero flag in bit 6 and the carry flag in bit 0*/
  rflags &= ~(1 << 6);
  rflags &= ~1;
  vp->reg_storage.eflags = rflags;  
  /* Write the new rflags back to the guest */
  vmwrite(GUEST_RFLAGS, rflags);
  
  /* Free the temporary mappings */
  vmem_free_temp((uint64_t*)vaddr, PAGE_SIZE);
  vkdirty(vk_heap, (void*)vaddr, 1);
  
  /* relaunch the guest */
  restore_gpregs(vp);
  launch_vproc(vp);

  return;
}

static void control_register_read_handler( uint64_t qualification ){

  uint64_t cr_reg_number, cr_reg;
  uint8_t type;

  cr_reg_number = qualification & 0xF;
  cr_reg = (qualification & 0x700) >> 8; /* bits 8-10 again */
  type   = (qualification & 0x30) >> 4;  /* bits 5-6 */

  switch (type) {
    case 0:
      kprintf("Attempt to WRITE CR: MOV %s, CR %d", 
	      vmx_ctrl_reg_access_name_list[cr_reg], cr_reg_number);
      break;
    case 1:
      kprintf("Attempt to READ CR: CR %d, MOV %s:",
	      cr_reg_number, vmx_ctrl_reg_access_name_list[cr_reg]);
      break;
    case 2:
      kprintf("CLTS");
      break;
    case 3: 
      kprintf("LMSW\n");
      if(qualification & 0x20) {
	kprintf("Memory");
      } else {
	kprintf("Register");
      }
      break;
  }

  return;
}

static void debug_register_access_handler( uint64_t qualification ){

  uint64_t cr_reg, dr_reg;

  if(qualification & 0x08)
    kprintf("read from debug register");
  if(!(qualification & 0x08))
    kprintf("Mov to debug register");
  /* bits 8-10 again tell which GP register */
  cr_reg = (qualification & 0x700) >> 8;
  /* bits 0-2  tell # of debug register */
  dr_reg = (qualification & 0x7);

  kprintf("Attempt to READ Debug register # %d, GP register %s:",
	  dr_reg, vmx_ctrl_reg_access_name_list[cr_reg]);

  return;
}

static void io_read_write_handler( uint64_t qualification ){

  int access_size;

  access_size = qualification & 0x7;
  switch (access_size) {
    case 0:
      access_size = 1;
      break;
    case 1:
      access_size = 2;
      break;
    case 3:
      access_size = 4;
      break;
  }

  return;
}

/* rdmsr handler returns the value of the msr to the guest                 */
/* rdmsr is a 64 bit instruction that takes the value in rcx (msr to read) */
/* for backwards compatability stores it in the form edx:eax               */
static void rdmsr_handler( vproc_t *vp ){
  uint64_t msr_value_to_read, ins_len, vp_RIP;

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     a rdmsr */
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);

  /* The msr that was going to be read is stored in the guest RCX register */
  msr_value_to_read = read_msr(vp->reg_storage.rcx);

  /* Format our 64 bit value from  our read msr function into edx:eax form */
  vp->reg_storage.rdx = ( msr_value_to_read >> 32 );
  vp->reg_storage.rax = ( msr_value_to_read & 0xFFFFFFFF );

  /* now relaunch the vproc with the value reutrned by rdmsr, but past the
     actual rdmsr instruction that caused the vmexit */
  restore_gpregs(vp);
  launch_vproc(vp);

  return;
}

static void vm_guest_state_handler( uint64_t qualification ){

  uint64_t rflags;

  kprintf("VM-entry failure due to invalid guest state. Qualification was: %d",
	  qualification);

  vmread(GUEST_RFLAGS, &rflags);
  kprintf("Error: RFLAGS - %x\n", rflags);
  if(qualification == 2)
    kprintf("problem loading PDPTE's");
  if(qualification == 3)
    kprintf("failure due to and attempt to inject NMI that is blocking");
  if(qualification == 4)
    kprintf("VMCS link pointer invalid");

  return;
}

static void vm_msr_loading_handler( uint64_t qualification ){

  kprintf("VM-entry failure due to MSR loading");
  kprintf("MSR load area which failed %d", qualification);

  return;
}

static void ept_violation_handler( uint64_t qualification, vproc_t *vp){

  uint64_t address, RIP;

  if(qualification & 0x1)
    kprintf("violation was EPT read\n");
  if(qualification & 0x2)
    kprintf("violation was EPT write\n");
  if(qualification & 0x4)
    kprintf("violation was EPT instruction fetch\n");
  if(qualification & 0x80)
    vmread(GUEST_LINEAR_ADDRESS, &address);

  kprintf("Linear Address access violation at %X\n", address);

  vmread(GUEST_RIP, &RIP);
  vmread(GUEST_PHYSICAL_ADDRESS, &address);

  kprintf("Physical Address access violation at %x\n", address);
  kprintf("Instruction pointer: %x\n", RIP);
  kprintf("ept says 0x%x\n", ept_walk(address, vp->peptp, 0, 0, 0));

  asm volatile("hlt");

  return;
}

/* To enable APIC debuging set "#define APIC_DEBUG" at top of file.        */
/* WARNING: The APIC fires constantly! So, halt somewhere in the guest to  */
/* to debug your code effectively                                          */
static void apic_access_handler( vproc_t *vp ){
  uint64_t qualification, ins_len, vp_RIP;

  /* Take care of the fact that we need move past the instruction that
     caused a vmexit in the first place. In this case it is the size of
     an apic access */  
  vmread(VM_EXIT_INSTRUCTION_LEN, &ins_len);
  vmread(GUEST_RIP, &vp_RIP);
  vmwrite(GUEST_RIP, vp_RIP + ins_len);  

#ifdef APIC_DEBUG
  kprintf("[HYPV APIC] Entered handler\n");
#endif


  /*Table 27-6 in the vmexit section chapter 27 of the intel manual describes*/
  /*The qualification field is where this information is populated           */
  /*the access type that caused a vmexit. 0x0000 = read, 0x1000 = write,     */
  /*others are not currently handled        ^ (read bit)   ^ (write bit)     */
  vmread(EXIT_QUALIFICATION, &qualification);

#ifdef APIC_DEBUG
  kprintf("[HYPV APIC] qualification 0x1000 write 0x0000 read we got a 0x%x\n", (qualification & 0x1000));
  kprintf("[HYPV APIC] apic field was 0x%x\n", (qualification & 0xfff));
#endif

  /* For now we assume the guest has complete control of the core it is      */
  /* configuring. Thus, while we are catching what the guest would like to do*/
  /* we do not restrict what the guest is doing. Some checks should be added */
  /* to prevent potential out of bounds behaviour, but the main push for this*/
  /* is motivated by the drive to give the guest complete control of the core*/
  /* it is using. We are not enabling external interrupt exiting and we are  */
  /* leaving the exception bitmap zeroed out. This allows the guest IDT to   */
  /* handle all of the interrupts it is receiving. This is also a performance*/
  /* improvment as we skip coming into the hypervisor for timer interrupts.  */
  /* The only real emulation we should see here is the aknowledgment of EOI, */
  /* but we should be able to elimintate that with to incoming intel updates.*/
  if((qualification & 0x1000) == 0x1000){

#ifdef APIC_DEBUG
    kprintf("[HYPV APIC] APIC write caught\n");
    kprintf("[HYPV APIC] data: 0x%x field: 0x%x\n",
    	    (qualification & 0xfff), vp->reg_storage.rsi);
#endif

    if((qualification & 0xfff) == APIC_ICRL){

#ifdef APIC_DEBUG
      kprintf("[HYPV APIC] caught ICRL\n");
      kprintf("[HYPV APIC] core to write to in APIC_ICRH 0x%x\n", 
      	      lapic_read(APIC_ICRH));
#endif
      /* If it is an init signal start the count for joining a core*/
      if( vp->reg_storage.rsi == APIC_INIT){

#ifdef APIC_DEBUG
	kprintf("[HYPV APIC] caught INIT signal\n");
#endif

	count_startup_ipis++;	
      }/* We received a SIPI count the first*/
      else if( (vp->reg_storage.rsi & 0xf00) == APIC_STARTUP){

#ifdef APIC_DEBUG
	kprintf("[HYPV APIC] caught SIPI signal\n");
#endif

	count_startup_ipis++;

#ifdef APIC_DEBUG
	kprintf("[HYPV APIC] number of signals %d\n",count_startup_ipis);
#endif
	/*Second SIPI received time to join a core to the guest */
	if(count_startup_ipis == 3){

#ifdef DEBUG_SMP
	  kprintf("[HYPV APIC] send ipi\n");
#endif
	  count_startup_ipis = 0;

#ifdef APIC_DEBUG
	  kprintf("[HYPV APIC] whom to send ipi to 0x%x\n", 
		  lapic_read(APIC_ICRH) >> 24);
#endif
	  /*IPI's don't have a lot of room for data. So, we use a global  */
	  /*vproc pointer to keep track of who we are joining to          */
	  SIPI_vp = vp;
	  /*send the ipi to the guest the core wants to start             */
	  /*This is neat as we can read the previous ICRH write to figure */
	  /*out who we are trying to message                              */
	  send_ipi((lapic_read(APIC_ICRH) >> 24), HYPV_PSEUDO_SIPI);

	  /*wait for the core we are starting to acknowledge the IPI      */
	  while(lapic_read(APIC_ICRL) & APIC_DELIVS);

#ifdef DEBUG_SMP
	  kprintf("[HYPV SMP] Delivery Status 0x%x\n",
		  lapic_read(APIC_ICRL) & APIC_DELIVS);
#endif
	}
      }
    }
    else{/*write everything else to the APIC*/
      lapic_write((qualification & 0xfff), vp->reg_storage.rsi);
    }
  }  /*else if reads are harmless*/
  else if((qualification & 0x1000) == 0x0000){
#ifdef APIC_DEBUG
    kprintf("[HYPV APIC] APIC read caught\n");
#endif
    vp->reg_storage.rax = lapic_read((qualification & 0xfff));
  }
  else{/* else disaster strikes */
    kprintf("[HYPV APIC] unknown APIC access caught\n");
    panic();
  }

  restore_gpregs(vp);
  launch_vproc(vp);
  
  return;
}

static void ept_misconfiguration_handler( vproc_t *old_vp ){

  return;
}

static void vmx_preemption_handler( vproc_t *old_vp ){

  switch_vprocs();

  /* Does not return */
}

/* order of ops  RDI  RSI  RDX RCX */
	
void vmexit_handler() {
  uint64_t qualification;
  uint64_t int_info;
  uint64_t exit_reason;
  vproc_t *vp;
  uint64_t old_vmcs_ptr;	
  uint64_t vectoring_info;

  (void)vmptrst(&old_vmcs_ptr);
  vp = vproc_getby_vmcs(old_vmcs_ptr);
  save_gpregs(vp);
  /* Save stack and instruction pointers, in case we need to modify them. */
  vmread(GUEST_RIP, &vp->reg_storage.rip);
  vmread(GUEST_RSP, &vp->reg_storage.rsp);
  vmread(GUEST_CR3, &(vp->vmcs.GUEST_CR3));
  vmread(VM_EXIT_REASON, &exit_reason);
  vmread(EXIT_QUALIFICATION, &qualification);
  vmread(VM_EXIT_INTR_INFO, &int_info);
  vmread(IDT_VECTORING_INFO, &vectoring_info);

  if(exit_reason & (1 << 31)) {
    kputs("VM-entry failure!");
    exit_reason &= ~(1 << 31);
  }

  switch(exit_reason) {
    case 0:  /* Exception or NMI */
      exception_nmi_handler( int_info, qualification, vectoring_info );
    case 1:  /*external interrupt */
      external_interrupt_handler ( vp, int_info );
      break;
    case 3:  /*INIT IPI */
      kprintf("INIT signal trapped by hyervisor\n");
      break;
    case 4:  /* SIPI IPI */
      kprintf("SIPI signal trapped by hyerpvisor\n");
      break;
    case 7:  /* Interrupt window exiting */
      interrupt_window_exiting_handler( vp );
      break;	
    case 9:  /* hardware task switch */
      break;
    case 10: /* CPUID */
      cpuid_handler( vp );
      break;
    case 14: /* invlpg */
      kprintf("invlpg trapped by hypervisor\n");
      break;
    case 18: /* VMCALL*/
      vmcall_handler( vp );
      break;
    case 19: /* vmclear */
      vmclear_handler( vp );
      break;
    case 20: /* vmlaunch */
      vmlaunch_handler( vp );
      break;
    case 21: /* vmptrload */
      vmptrload_handler( vp );
      break;
    case 22: /* vmptrst */
      kprintf("VMPTRST failure\n");
      break;
    case 23: /* vmread */
      kprintf("VMREAD failure\n");
      break;
    case 24: /* vmresume */
      kprintf("VMRESUME failure\n");
      break;
    case 25: /* vmwwrite */
      vmwrite_handler( vp );
      break;
    case 26: /* vmxoff */
      kprintf("VMXOFF failure\n");
      break;
    case 27: /* vmxon */
      vmxon_handler( vp );
      break;
    case 28: /* CR register read */
      control_register_read_handler( qualification );
      break;
    case 29: /* debug register access*/
      debug_register_access_handler( qualification );
      break;
    case 30: /* I/O read write */
      io_read_write_handler( qualification );
      break;
    case 31: /* rdmsr */
      rdmsr_handler( vp );
    case 33: /* VM entry failure guest state */
      vm_guest_state_handler( qualification );
      break;
    case 34:/* VM entry failure MSR loading */
      vm_msr_loading_handler( qualification );
      break;
    case 36: /* MWAIT */
      break;
    case 44:/* APIC access */
      /* see vol 3b 24-10 table 24-6 */
      apic_access_handler( vp );
      break;
    case 48: /* EPT violation */
      ept_violation_handler( qualification, vp );
      break;
    case 49: /*EPT Misconfigured */
      ept_misconfiguration_handler( vp );
      break;
    case 52: /* VMX Preemption */
      vmx_preemption_handler( vp ); /* Does not return */
      break; 		
  }
  return;
}
