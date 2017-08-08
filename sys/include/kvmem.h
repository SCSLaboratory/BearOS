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

#pragma once

#include <constants.h>          /* For and private */
#include <stdint.h>             /* For std types          */
#include <memory.h>             /* For memory structures  */
#include <proc.h>               /* For Proc_t*            */
#include <pci.h>

/* 
 * TLB sections to flush (only TLB_ALL currently implemented).
 * The real solution is PCID...
 */
#define TLB_PL0     0
#define TLB_PL1     1
#define TLB_PL2     2
#define TLB_PL3     3
#define TLB_SCRATCH 4
#define TLB_ALL     5

/* Size of every buffer doled out by dmapool_alloc() */
#define DMAPOOL_BUF_SIZE  (2048)

/* Number of buffers available to hand out. */
#define DMAPOOL_BUF_COUNT (4096)

/* Total size of DMA pool. */
#define DMAPOOL_SIZE (DMAPOOL_BUF_SIZE * DMAPOOL_BUF_COUNT)

/******************************************************************************
 ******************************** STRUCTURES **********************************
 *****************************************************************************/
/* Tracks resources for a single DMA memory region.  
 * Could add:
 *  - IOMMU support
 *  - Scatter/Gather support
 *  - Better allocation strategy (see dma_res_alloc in kvmem.c)
 */
struct dma_res {
	uint32_t sz;
	uint64_t vaddr;
	uint64_t paddr;
};

/******************************************************************************
 **************************** PUBLIC FUNCTIONS ********************************
 *****************************************************************************/

/* Initialize kernel's virtual memory */
void kvmem_init();

/* Mapping/Unmapping virtual memory */
int  attach_page  (uint64_t vaddr, uint64_t paddr, uint64_t ptflags);

/* Allocating/mapping/unmapping memory for peripheral device communication */
uint64_t kvmem_map_mmio(Proc_t *p, uint64_t virt_addr, Pci_bar_t *bar);
uint64_t kvmem_map_dma_region(Proc_t *p, uint64_t virt_addr, uint64_t pages);
void     kvmem_unmap_devmem(Proc_t *p);


/* Deallocating paging structures */
void detach_page(Proc_t *p, uint64_t vaddr);

/* Clear the translation cache */
void flush_tlb(int);
