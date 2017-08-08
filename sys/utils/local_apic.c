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
#include <pio.h>
#include <smp.h>
#include <apic.h>
#include <asm_subroutines.h>
#include <tsc.h>
#include <smp.h>
#include <semaphore.h>
/* adapted from  Plan9 */
 
uint32_t lapic_read(uint32_t offset){
  return *(uint32_t *)(lapicaddr+offset);
}

void lapic_write(uint32_t offset, uint32_t data){
  *(uint32_t *)(lapicaddr+offset) = data;
  return;
}

void lapic_timerintr(void){
  if(lapicaddr)
    lapic_write(APIC_EOI, 0);

  return;
}

void cpuSetAPICBase(uint64_t apic){
  uint64_t msr = 0;
  msr = (apic & 0xfffff000) | IA32_APIC_BASE_MSR_ENABLE;
  write_msr(IA32_APIC_BASE_MSR,msr);

  return;
}
 
uint64_t cpuGetAPICBase(){
  return (read_msr(IA32_APIC_BASE_MSR) & 0xfffff000);
}



void apic_set_timer(uint32_t cycles, int apic_divisor){

  int tmr_options;
	
  /* set as periodic and the local vector to 32 */
  tmr_options  = 	32 | TMR_PERIODIC;
		
  lapic_write(APIC_LVT_TMR,tmr_options);	
  /*set the divisor for the apic*/
  lapic_write(APIC_TMRDIV,apic_divisor);
	
  /*the interrupt will fire at cycles/divisor */
  lapic_write(APIC_TMRINITCNT, cycles/apic_divisor);
	
  return;
}


int calibrate_apic_timer(void){
  unsigned apic, apic_start;
  uint64_t tsc_hz, tsc, tsc_start, HZ;

  tsc_hz = get_tsc_freq();
 
#ifdef DEBUG
  kprintf("tsc_frequency = %u MHz \n ", ((tsc_hz/1000)/1000));
#endif

  /* This is the frequency the timer will fire.
   * This sets up how fast we switch processes 
   */
  HZ  = tsc_hz / 100;//0;
  /* TODO: on 9010s, dividing by 10000 causes thrashing */

  /* place a stupid long value in the apic timer so it starts counting
   * we can then calibrate it against the tsc known frequency 
   */
  apic_set_timer(4000000000, APIC_TDIV_1);

  /*read the intial values */
  apic_start = lapic_read(APIC_TMRCURRCNT);
  tsc_start = readtsc();
 
  /*Loop for the known quantum of time based on the tsc freq */
  do{
    tsc = readtsc();
    apic = lapic_read(APIC_TMRCURRCNT);
  }while ((tsc - tsc_start) < HZ );

  /*calculate the number of apic cycles that occured in the fixed time window
   * and set the apic timer interrupt to fire on those now.  
   */
  apic_set_timer((apic_start - apic), APIC_TDIV_1); 

  return 0;
}

void lapic_init(){
  
  if(!lapicaddr)
    return;

  lapic_write(APIC_DFR,0xFFFFFFFF); 
  lapic_write(APIC_LDR,(lapic_read(APIC_LDR)&0x00FFFFFF)|1);
  lapic_write(APIC_LVT_TMR,APIC_DISABLE);
  lapic_write(APIC_LVT_PERF,APIC_NMI);
  lapic_write(APIC_LVT_LINT0,APIC_DISABLE);
  lapic_write(APIC_LVT_LINT1,APIC_DISABLE);
  lapic_write(APIC_TASKPRIOR,0);

  cpuSetAPICBase(cpuGetAPICBase());

  /*turn on the apic and route surpious ints to a black hole */
  lapic_write(APIC_SPURIOUS,63|APIC_SW_ENABLE);

#ifdef DEBUG
 kprintf("APIC ID 0x%x \n", ((lapic_read(APIC_APICID)>>24) & 0xFF));
#endif

#ifdef KERNEL
      calibrate_apic_timer();
#endif

  /*writing a 0 to the high ICR sets it to LAPIC 0 BSP usually.             */
  /*The low ICR tells the APIC what to to do. In this case it sends an init */
  /*level de-assert to syynchronize arbitration ID's. The following is while*/
  /*is the wait for the response.                                           */
  lapic_write(APIC_ICRH, 0);
  lapic_write(APIC_ICRL, APIC_ALLINC | APIC_LEVEL | APIC_DEASSERT | APIC_INIT);
  while(lapic_read(APIC_ICRL) & APIC_DELIVS);

  return;
}

/*May need to be used in the future*/
void lapic_enableintr(void){
  if(lapicaddr)
    lapic_write(APIC_TASKPRIOR, 0);

  return;
}

void lapic_disableintr(void){
  if(lapicaddr)
    lapic_write(APIC_TASKPRIOR, 0xFF);

  return;
}

void lapic_eoi(void){
  if(lapicaddr)
    lapic_write(APIC_EOI, 0);

  return;
}

uint32_t this_cpu(void){
  int x;
  if(lapicaddr)
    x = (lapic_read(APIC_APICID)>>24) & 0xFF;
  else
    x = 0;
  return x;
}

void delay(int n){
  int i;
  for(i = 0; i < n*100; i++)
    asm volatile("nop");

  return;
}


/*Intel startup algorithm */
void lapic_start_aps(uint8_t apicid, uint32_t addr){

#ifdef DEBUG
  kprintf("[APIC] starting apicid = %x at addr = %x\n",apicid,addr);
#endif
  lapic_write(APIC_ICRH, apicid<<24);
  lapic_write(APIC_ICRL, APIC_INIT );
  delay(200);   

  lapic_write(APIC_ICRH, apicid<<24);
  lapic_write(APIC_ICRL, APIC_STARTUP | (addr>>12));

  delay(200);
  lapic_write(APIC_ICRH, apicid<<24);
  lapic_write(APIC_ICRL, APIC_STARTUP | (addr>>12));
	
  delay(100);

  return;
}

void send_ipi(uint8_t apicid, uint32_t int_vector){

  //  kprintf("entered send_ipi on core %d w dest core %d and vector %d\n", 
  //	  this_cpu(), apicid, int_vector);

  lapic_write(APIC_ICRH, apicid<<24);
  lapic_write(APIC_ICRL, APIC_ASSERT | int_vector);

  //  kprintf("leaving send_ipi\n");

  return;
}
