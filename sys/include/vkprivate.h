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
 * private macros used ONLY inside the vk module
 */

#define FREE   (0b00000000)
#define ALLOC  (0b10000000)  
#define DIRTY  (0b11000000)  
#define INVFREEMASK (0b11000000)
#define BITSPERPAGE 4  

/* pages are indexed from 0 */
#define vksize(hp) (*((uint32_t*)hp))                      /* number of pages */
#define vkpagesize(hp) (*(((uint32_t*)hp)+1))              /* size of each page */
#define vkmapsize(hp) ((vksize(hp)+(BITSPERPAGE)-1)/BITSPERPAGE) /* pagemap size in bytes */
#define vkmap(hp) ((uint8_t*)(hp+(2*sizeof(uint32_t))))    /* byte ptr to map location */
#define vkpage(hp,i) ((uint8_t*)hp+((i+1)*vkpagesize(hp))) /* byte ptr to page index 0,1,2,... */

/* page map access */
#define pageindex(hp,p) (((int)((p-hp)/vkpagesize(hp)))-1)       /* index of a page in page map */
#define pagebyte(hp,page) (vkmap(hp) + (page/BITSPERPAGE))
#define pagebits(page,val) (val>>(2*(page%BITSPERPAGE)))

/* retrieve the pages bits in the top of a byte */
#define getbits(hp,page) (((*pagebyte(hp,page))<<(2*(page%BITSPERPAGE))) & (0b11000000))

/* page state change by oring in bits at the pages position in the map*/
#define allocpage(hp,page) ((*pagebyte(hp,page)) |= (pagebits(page,ALLOC)));
#define dirtypage(hp,page) ((*pagebyte(hp,page)) |= (pagebits(page,DIRTY)));

/* shift 1's to the right position using pagebits, then take the
 * inverse to put 0s in the mask & 1s everywhere else, then apply
 * the mask to free the page
 */
#define freepage(hp,page)  ((*pagebyte(hp,page)) &= (~(pagebits(page,INVFREEMASK))));

/* booleans */
#define isfree(hp,page) ((getbits(hp,page))==FREE)
#define isalloc(hp,page) ((getbits(hp,page))==ALLOC)
#define isdirty(hp,page) ((getbits(hp,page))==DIRTY)

/* PRIVATE FUNCTIONS */
static int allfree(vkheap_t* hp,int start,uint32_t size);
static void showstate(vkheap_t *hp,uint32_t page);


