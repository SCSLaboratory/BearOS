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

/* diversity.h -- functions for implementing diversity in the hypervisor and
 *                kernel.
*/
#pragma once
#include <memory.h>
#include <stdint.h>
#include <sha256.h>

#if defined(KERNEL) && defined(KPLT)
uint64_t kplt;
#endif


/* static partition structure definition */
struct partition_type {
  uint64_t start;
  uint64_t end;
  struct partition_type *next;
  struct partition_type *prev;
};

typedef struct partition_type partition_t;


/* A similar function given to the diversifier by the kernel or hypervisor. It
 * clones a page from the given (v)proc's address space into the new 
 * diversified address space.
 *
 * Argument 1: the (v)proc structure.
 * Argument 2: The new PML4T entry.
 * Argument 3: The virtual address of the page to be cloned.
 * Argument 4: The virtual address to place the newly-cloned page at.
*/

typedef void (*diversity_page_cloner)(void *, union pt_entry *, uint64_t, uint64_t);

/* Similar to the above, but copies a page to a new address in the same address
 * space.
 *
 * Argument 1: The (v)proc structure.
 * Argument 2: The virtual address of the page to be cloned.
 * Argument 3: The virtual address to place the newly-cloned page at.
*/
typedef void (*diversity_page_copier)(void *, uint64_t, uint64_t);

void diversify(void *, SHA256_CTX *ctx);
void replicate(void *, diversity_page_copier);

/* access module that can profile all of memory and let you find random 
   pieces of memory for diversity-type reasons. */

uint64_t pick_random_place(partition_t *parts, uint64_t size, 
			   uint64_t align, partition_t **p);
partition_t *init_partitions(void);
void free_partitions(partition_t *parts);
void split_partition(partition_t *part, uint64_t start, uint64_t end);
void add_to_partitions(partition_t *parts, uint64_t start, uint64_t size);
