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

/* based on plan9 mit kernel */

#include <constants.h>
#include <smp.h>
#include <apic.h>
#include <file_abstraction.h>
#include <fatfs.h>
#include <acpi.h>
#include <kqueue.h>
#include <kstring.h>

static volatile uint32_t num_ap_cpus;
extern uint64_t* kstack;

void set_running() {
  num_ap_cpus++;
}

/**TODO redo for new memory system*/
#define checkstatus(x) if(x) fs_error(x)
void smp_boot_aps(void)
{
  /*kernel programming is fun, if you can tell me why this is important I'll 
    give you a dollar -Rob*/
  /* Answer I need this value not to change on the BSP core. When other cores 
     run this value can change. */

  /* the other cores update num_ap_cpus, which advances the do while loop in 
     this function*/

  uint32_t tramp_size, fstatus;
  static volatile uint32_t i; 
  FIL ctx;
  uint64_t *boot_code_ptr;
  FILINFO stat;
  void* Qvalptr;
  struct APICLocalAPICEntry *apic;

  boot_code_ptr = (uint64_t*)(0x6000);
  num_ap_cpus = 0;
  fstatus = 0;	

#ifdef DEBUG_SMP
  kprintf("[SMP] boot aps\n");
#endif
  f_stat("/tramp", &stat);
	
#ifdef DEBUG_SMP
  kprintf("[SMP] tramp stated\n");
#endif
  f_open(&ctx,"/tramp",FA_READ);

#ifdef DEBUG_SMP
  kprintf("[SMP] tramp opened\n");
#endif
	
  tramp_size = stat.fsize;
  if(tramp_size % PAGE_SIZE)
    tramp_size += PAGE_SIZE - (tramp_size % PAGE_SIZE);

  kmemset(boot_code_ptr,0,tramp_size);

  f_read(&ctx,boot_code_ptr,tramp_size,&fstatus);
#ifdef DEBUG_SMP
  kprintf("[SMP] tramp read in\n");
#endif
  f_close(&ctx);
	
#ifdef DEBUG_SMP
  kprintf("[SMP] tramp closed\n");
#endif

  i = 0;
#ifdef DEBUG_SMP
  kprintf("[SMP] boot strap cpu APICID = %x\n",this_cpu());
#endif
  while( (Qvalptr = qget(CPUQueue)) ) {
    apic = (struct APICLocalAPICEntry*)Qvalptr;
    if (apic->APICID == this_cpu() || apic->APICID == 0)
      continue;

    temp_stack = new_stack(NULL); 
		
    kmemset(temp_stack-KSTACK_PAGES*PAGE_SIZE,
	    0,sizeof(KSTACK_PAGES*PAGE_SIZE));

#ifdef DEBUG_SMP
    kprintf("[SMP] start core with APICID %d\n",apic->APICID);
#endif

    /* i <= (maximum number of cores wanted - 1) */
    if ( i <= (MAX_CORES - 1) ) {
      lapic_start_aps(apic->APICID, 0x6000);
      while(num_ap_cpus == i);
      i = num_ap_cpus;
    }
  }


#ifdef DEBUG_SMP
  kprintf("[SMP] %d cores online\n", (i+1));
#endif
  return;
}
