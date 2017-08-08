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
/* 
 * vk.h - the public interface to the vk module
 * 
 */

typedef void vkheap_t;
typedef void vkpage_t;

/* vkinit -- constructs a heap at hp, containing heapsize contiguous
 * pages, each of size pagesize; hp must be pagesize aligned 
 */
void vkinit(vkheap_t *hp,uint32_t heapsize,uint32_t pagesize);

/* vkmalloc -- returns a pointer to 'size' contiguous pages on a vkheap.  */
uint64_t vkmalloc(vkheap_t* hp,uint32_t size);

/* vkdirty -- makes the 'size' pages beginning at 'p' dirty. */
void vkdirty(vkheap_t* hp, vkpage_t* p,uint32_t size);

/* vkfreedirty -- makes all dirty pages in a vkheap free for reuse */
void vkfreedirty(vkheap_t* hp);

/* vkfree -- makes the 'size' pages beginning at 'p' free */
void vkfree(vkheap_t* hp, vkpage_t *p, uint32_t size);

/* vkshowmap -- shows the page map */
void vkshowmap(vkheap_t *hp);

