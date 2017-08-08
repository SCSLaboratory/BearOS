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
#include <kstring.h>
#include <constants.h>

/* This is pretty dumb.
 * We can optimize this with SSE later on.
*/
void *kmemset(void *s, int c, size_t n) {
  int8_t *p = (int8_t *)s;

  if(n < 0) return s;
  while(n) {
    *p++ = (int8_t)c;
    n--;
  }
  return s;
}

void *kmemcpy(void *d, const void *s, size_t n) {
  asm("movq %0,%%rsi \n\t"
      "movq %1,%%rdi \n\t"
      "movl %2,%%ecx \n\t"
      "rep movsb"
      :: "p"(s), "p"(d), "m"(n)
      : "%rsi", "%rdi", "%ecx", "memory");
  
  return d;
}

/* Swaps every two bytes in an arbitrary data buffer. */
void kswab(void *buf, size_t len) {
  uint8_t *bytes = (uint8_t *)buf;
  uint8_t tmp;
  size_t i;

  for(i = 0; i < len; i += 2) {
    tmp = bytes[i+1];
    bytes[i+1] = bytes[i];
    bytes[i] = tmp;
  }
}

/* From BSD source see their license 
 * return an integer greater than, equal to, or
 * less than 0, according as the string s1 is greater than,
 * equal to, or less than the string s2.
*/
int kstrncmp(const char *s1, const char *s2, size_t n){
  if (n == 0)
    return (0);
  do {
    if (*s1 != *s2++)
      return (*(const unsigned char *)s1 -
	      *(const unsigned char *)(s2 - 1));
    if (*s1++ == 0)
      break;
  } while (--n != 0);
  return (0);
}

size_t kstrlen(const char *str){
  register const char *s;
  
  for (s = str; *s; ++s);
  return(s - str);
}

char * kstrncpy(char *dst, const char * src, size_t n){
  if (n != 0) {
    register char *d = dst;
    register const char *s = src;
    
    do {
      if ((*d++ = *s++) == 0) {
	/* NUL pad the remaining n-1 bytes */
	while (--n != 0)
	  *d++ = 0;
	break;
      }
    } while (--n != 0);
  }
  /* null terminated even if sum of lengths is greater than n */
  *(dst+(n-1))=0;
  return (dst);
}

char *kstrncat(char *dst, const char *src, size_t n)
{
  if (n != 0) {
    register char *d = dst;
    register const char *s = src;

    while (*d != 0)
      d++;
    do {
      if ((*d = *s++) == 0)
	break;
      d++;
    } while (--n != 0);
    *d = 0;
  }
  /* null terminated even if sum of lengths is greater than n */
  *(dst+(n-1))=0;
  return (dst);
}



/* Generic sorting interface. Requires a function to make comparison of elements
 * and a function for swapping elements. */
static void kqsort(uint8_t *, size_t, int, int, int (*)(void *, void *), void (*)(void *, void *));
static int kqsort_partition(uint8_t *, size_t, int, int, int (*)(void *, void *), void (*)(void *, void *), int);
void ksort(void *array, size_t num, size_t size, int (*cmp)(void *, void*), void (*swap)(void *, void*)) {
	kqsort(array, size, 0, num-1, cmp, swap);
}

/* Quicksort implementation.
 * FIXME: This blows stack whenever we turn debugging symbols on! Get a more
 * resilient sort! */
#define QELEMENT(array, pos, size) ((array)+((pos)*(size)))

static void kqsort(uint8_t *bytes, size_t size, int left, int right,
                  int (*cmp)(void *, void *), void (*swap)(void *, void *)) {
	if(left < right) {
		int pividx = (right + left) / 2;
		pividx = kqsort_partition(bytes, size, left, right, cmp, swap, pividx);
		kqsort(bytes, size, left, pividx-1, cmp, swap);
		kqsort(bytes, size, pividx+1, right, cmp, swap);
	}
}

static int kqsort_partition(uint8_t *bytes, size_t size, int left, int right,
                           int (*cmp)(void *, void *), void (*swap)(void *, void *),
                           int pividx) {
	uint8_t pivot[size];
	int i;
	int storeidx;

	kmemcpy(pivot, QELEMENT(bytes, pividx, size), size);
	swap(QELEMENT(bytes, pividx, size), QELEMENT(bytes, right, size));
	storeidx = left;
	for(i = left; i < right; i++) {
		if(cmp(bytes + i*size, pivot)) {
			swap(QELEMENT(bytes, i, size), QELEMENT(bytes, storeidx, size));
			storeidx++;
		}
	}
	swap(QELEMENT(bytes, storeidx, size), QELEMENT(bytes, right, size));

	return storeidx;
}

/* from string.c */

#define cmp_preconditions(s1, s2) {                             \
        if((s1) == NULL && (s2) == NULL) return 1;      \
        if((s1) == NULL || (s2) == NULL) return 0;      \
}

/* Returns 1 for equal, 0 for not equal */
int kstreq(const char *s1, const char *s2) {
        cmp_preconditions(s1, s2);

        while(*s1 == *s2 && *s1 && *s2) {
                s1++;
                s2++;
        }

        if(*s1 == '\0' && *s2 == '\0') return 1;
        return 0;
}

int kcontains_start(const char *haystack, const char *needle) {
        cmp_preconditions(needle, haystack);

        while(*needle == *haystack && *needle && *haystack) {
                needle++;
                haystack++;
        }

        if(*needle == '\0') return 1;
        return 0;
}

void bcopy(const void *src, void *dst, size_t len)
{
        const char *s = src;
        char *d = dst;

        while (len-- != 0)
                *d++ = *s++;
}

