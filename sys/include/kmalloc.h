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

#include <stdint.h>
#include <kmalloc_sites.h>

/* initialize the heap at location heap with heapsize words */
void kheapinit(uint64_t *heap,int heapsize);

/* malloc and free */
void *kmalloc(int bytes);
int kfree(void *p);

#ifdef KMALLOC_TRACKING
void *kmalloc_track_site(int site, int bytes);
int kfree_track_site(int site, void *p);
void kmalloc_clear_sites(void);
#endif

/* check the heap; print statistics if either an error found or printout=1 */
int kheapcheck(int printout,char *msg);

/* calloc -- mallocs and initializes -- used by lwip */
void *kcalloc(int bytes);
/* realloc -- grows a piece of memory */
void *krealloc(void *mem_old, int size_new);

