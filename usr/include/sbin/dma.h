#include <stdint.h>

/*******************************************************************************
 ********************************* STRUCTURES **********************************
 ******************************************************************************/

/* 
 * Tracks resources for a single DMA memory region.  
 * Could add:
 *  - IOMMU support
 *  - Scatter/Gather support
 *  - Better allocation strategy (see dma_res_alloc in kvmem.c)
 */
#ifdef USER
struct dma_res {
        uint32_t sz;
        uint64_t vaddr;
        uint64_t paddr;
};

/* Size of every buffer doled out by dmapool_alloc() */
#define DMAPOOL_BUF_SIZE  (2048)

/* Number of buffers available to hand out. */
#define DMAPOOL_BUF_COUNT (4096)

/* Total size of DMA pool. */
#define DMAPOOL_SIZE (DMAPOOL_BUF_SIZE * DMAPOOL_BUF_COUNT)

void dma_heapinit(uint64_t *heap,int heapsize);

#endif
