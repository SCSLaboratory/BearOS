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

void *kmemset(void *s, int c, size_t n);
void *kmemcpy(void *d, const void *s, size_t n);
void kswab(void *, size_t);
char *kstrncpy(char *dst, const char * src, size_t n);
int kstrncmp(const char *s1, const char *s2, size_t n);
char *kstrncat(char *dst, const char *src, size_t n);
size_t kstrlen(const char *str);

/* Returns 1 on equality, 0 on inequality. Strings must be NULL terminated. */
int kstreq(const char *s1, const char *s2);

/* Is needle contained in the start of haystack? */
int kcontains_start(const char *haystack, const char *needle);
void bcopy(const void *src, void *dst, size_t len);
