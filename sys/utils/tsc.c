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

#include <stdint.h>
#include <asm_subroutines.h>

inline uint64_t readtscp() {
  uint32_t lo, hi;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx" );
/* TODO emulate cpuid in hypv */
//  asm volatile("cpuid");
  return (uint64_t)(lo) | ((uint64_t)(hi) << 32); 
}

inline uint64_t readtsc() {
  uint32_t lo, hi;
 // asm volatile("cpuid");
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi) :: "rcx" );
  return (uint64_t)(lo) | ((uint64_t)(hi) << 32); 
}

uint64_t get_tsc_freq(){

  uint64_t perf_stat_msr, platform_msr, flex_msr; 
  uint64_t ratio, flex_ratio_max, flex_ratio_min, flex_ratio_cur;
  uint64_t tsc_freq;
	
  perf_stat_msr = read_msr(0x198);
  platform_msr  = read_msr(0xCE);
  flex_msr      = read_msr(0x194); 

#ifdef DEBUG_TSC			
  kprintf("[TSC] Flex msr %x \n", flex_msr);
  kprintf("[TSC] Perf stat msr %x \n", perf_stat_msr);
  kprintf("[TSC] Platform msr %x \n", platform_msr);
#endif
	
  /*per the intel manuals and various online sites. Bit 31 of the performance
   *stats register indicates xe is running if so read the ratio from the perf 
   *stat msr if it's not running the value is in the platform msr
   */
  if(((perf_stat_msr >> 31) & 1)){
#ifdef DEBUG_TSC		
    kprintf("[TSC]XE mode is enabled\n");
#endif
    ratio = ((perf_stat_msr & 0x1F0000000000) >> 40);
  }
  else{
#ifdef DEBUG_TSC	
    kprintf("[TSC] XE is not enabled \n");
#endif
    ratio = (( platform_msr  & 0xFF00) >> 8);
  }

  /*the platform also has the min and max ratios */ 
  flex_ratio_min = ((platform_msr & 0x3F0000000000) >> 40);
  flex_ratio_max = ((platform_msr & 0xFF00) >> 8);
  /*the flex msr is theory has the current ratio */
  flex_ratio_cur = ((flex_msr & 0xFF00) >> 8);
#ifdef DEBUG_TSC		
  kprintf("[TSC] Ratio max %d, ratio min %d ratio cur %d\n", flex_ratio_max, flex_ratio_min, flex_ratio_cur);
#endif

  /*bit 16 of the flex msr tells us the ratio's can vary!!
   *however if the lower bits are zero we defult to the max
   *the frequency multiplier is 100 unless the bios is overclocked
   *I'm honestly not sure how to detect this other than 
   *noticing time goes off the rails
   */
  if(( flex_msr >> 16) & 1){
#ifdef DEBUG_TSC	
    kprintf("[TSC] flex ratios are active \n");
#endif
    if(flex_ratio_cur == 0){
      tsc_freq = flex_ratio_max * 100 * 1000 * 1000;
    }
    else{
#ifdef DEBUG_TSC		
      kprintf("[TSC] cur tsc ratio is valid \n");
#endif
      tsc_freq = flex_ratio_cur * 100 * 1000 * 1000;
    }
  }
  /*if theyr'e not flexing then just use the correct ratio from above*/
  else{
#ifdef DEBUG_TSC
    kprintf("[TSC]flex ratios are NOT active \n");
#endif
    tsc_freq = ratio * 1000 * 1000 * 100;	
  }

#ifdef DEBUG_TSC
  kprintf("[TSC] TSC frequency  %x \n", tsc_freq);
#endif
  return tsc_freq; 
}
