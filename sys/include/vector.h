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
/**
 * Original Author Hamid Alipour  modified by Steve Kuhn
 */
 
#include <kstdio.h>
#include <kstring.h>


 
#define VECTOR_INIT_SIZE    5
#define VECTOR_HASSPACE(v)  (((v)->num_elems + 1) <= (v)->num_alloc_elems)
#define VECTOR_INBOUNDS(i)	(((int) i) >= 0 && (i) < (v)->num_elems)
#define VECTOR_INDEX(i)		((char *) (v)->elems + ((v)->elem_size * (i)))
 
typedef struct _vector {
	void *elems;
	size_t elem_size;
	size_t num_elems;
	size_t num_alloc_elems;
    void (*free_func)(void *);
}vector;
 
/*initsialize a new vector */
 void vector_init(vector *, size_t, size_t, void (*free_func)(void *));
/*delete and free the memory for a vector */
 void vector_dispose(vector *);
/*inserrts an element  */
 void vector_insert(vector *, void *, size_t index);
 void vector_insert_at(vector *, void *, size_t index);
 void vector_push(vector *, void *);
 void vector_pop(vector *, void *);
 void vector_get(vector *, size_t, void *);
 void vector_remove(vector *, size_t);
 void vector_cmp_all(vector *, void *, int (*cmp_func)(const void *, const void *));
 void vector_copy(vector *, vector *);
 size_t vector_length(vector *);
 size_t vector_size(vector *);
 void vector_grow(vector *, size_t);
