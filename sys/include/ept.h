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
#include <memory.h>
#include <stdint.h>
#include <vmx.h>
#include <vproc.h>

/* Since there's no way we can validly define a "present" field, we'll do it   
 * as a macro. If a structure is marked read, write, or execute, it's present. 
 */
#define EPT_PRESENT(bits) ((bits) & 0x7)



void create_ept(vproc_t *vp);



/* Load the entire paging stack for a given virtual address, save the PML4T   
 * part which we often treat specially. The PML4T part is given as a parameter.
 */
uint64_t ept_walk(uint64_t,
                  uint64_t,
                  struct page_directory_pointer_table **,
                  struct page_directory **,
                  struct page_table **);

/* Functions for looking up page table structures. */
union ept_pt_entry *virt2ept_pml4t_entry(uint64_t eptp, uint64_t);
union ept_pt_entry *virt2ept_pdpt_entry (uint64_t eptp, uint64_t);
union ept_pt_entry *virt2ept_pd_entry   (uint64_t eptp, uint64_t);
union ept_page *virt2ept_pt_entry(uint64_t eptp, uint64_t);

void ept_map_page(uint64_t eptp, uint64_t gpaddr,
		  uint64_t paddr, uint8_t cache_type);

void ept_free_pml4t(struct page_map_level_4_table *pml4t);
void ept_free_pdpt(struct page_directory_pointer_table *pdpt);
void ept_free_pd(struct page_directory *pd);
void ept_free_pt(struct page_table *pt);
void ept_invalidate();
void destroy_ept(vproc_t *vp);

#ifdef XONLY_BEAR_HYPV
void mark_kernel_execute_only(vproc_t *vp);
#endif
