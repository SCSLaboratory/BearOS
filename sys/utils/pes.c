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

/******************************************************************************
 *
 * File: pes.c
 *
 * Description: Code for manipulating the processor extended state (PES) that
 *   is required for managing x87/MMX/SSE/SSE2/AVX/whatever's next.
 *   Note: This will change if we get a processor with AVX, AES-NI (I think),
 *    or more sophisticated add-ons.
 *
 *****************************************************************************/

#include <constants.h>
#include <kstring.h>
#include <stdint.h>
#include <pes.h>
#include <memory.h>
#include <kmalloc.h>
#include <vk.h>
#include <kvmem.h>

#define PES_SAVE_SIZE 512

/*** PRIVATE VARIABLES ***/
static char pes_gold_standard[PES_SAVE_SIZE];

/* PUBLIC FUNCTIONS */

/* DON'T USE FLOATING-POINT INSTRUCTIONS BEFORE THIS! */
int pes_init() {
	uint32_t mxcsr;

	asm volatile("fninit\n\t"
	             "stmxcsr %0\n\t"
	             "movl %0,%%eax\n\t"
	             "orl $0x1F80,%%eax\n\t"
	             "movl %%eax,%0\n\t"
	             "ldmxcsr %0\n\t"
	             "fxsaveq (%1)\n\t"
	             : "=m"(mxcsr)
	             : "r" (pes_gold_standard)
	             : "eax", "memory");
	return 0;
}

/******************************************************************************
 *
 * Function: pes_new_save
 *
 * Description: Returns a properly 16-byte-aligned FPU/MMX/SSE save area.
 *              Taking up a whole page is overkill (only 512B needed) but it's
 *              the easiest way to ensure alignment and deal with freeing
 *****************************************************************************/
#ifndef HYPV
char *pes_new_save(uint64_t *hack) {
#else 
char *pes_new_save() {
#endif

  void *s = (void*)vkmalloc(vk_heap, 1);
  vmem_alloc( (uint64_t*)s, PAGE_SIZE, PG_RW | PG_GLOBAL);

  kmemcpy(s, pes_gold_standard, PES_SAVE_SIZE);

  return (char *)s;
}

void pes_free_save(char *s) {
  vmem_free( (uint64_t*)s, PAGE_SIZE);
  vkdirty( vk_heap, (uint64_t*)s, 1);
  vkfreedirty( vk_heap );
  flush_tlb(0);
}

void pes_refresh_save(char *s) {
  kmemcpy(s, pes_gold_standard, PES_SAVE_SIZE);
}
