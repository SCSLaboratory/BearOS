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

/**
 * Original author Hamid Alipour modified by Steve Kuhn to work inside bear
 */
#include <vector.h>
#include <kmalloc.h>
#include <kstring.h>
#include <constants.h>
 
/*Freebsd memmove macro*/
#define memmove(d, s, l)        bcopy(s, d, l)

void vector_init(vector *v, size_t elem_size, size_t init_size, void (*free_func)(void *))
{
  v->elem_size = elem_size;
  v->num_alloc_elems = (int) init_size > 0 ? init_size : VECTOR_INIT_SIZE;
  v->num_elems = 0;
  v->elems = kmalloc_track(VECTOR_SITE,elem_size * v->num_alloc_elems);
  if(v->elems != NULL) /*this should be an assert but we don't have that */
    v->free_func = free_func != NULL ? free_func : NULL;
}
 
void vector_dispose(vector *v)
{
  size_t i;
 
  if (v->free_func != NULL) {
    for (i = 0; i < v->num_elems; i++) {
      v->free_func(VECTOR_INDEX(i));
    }
  }
 
  kfree_track(VECTOR_SITE,v->elems);
}
 
 
void vector_copy(vector *v1, vector *v2)
{
  v2->num_elems = v1->num_elems;
  v2->num_alloc_elems = v1->num_alloc_elems;
  v2->elem_size = v1->elem_size;
 
  v2->elems = krealloc(v2->elems, v2->num_alloc_elems * v2->elem_size);
  if(v2->elems != NULL)
    kmemcpy(v2->elems, v1->elems, v2->num_elems * v2->elem_size);
}
 
void vector_insert(vector *v, void *elem, size_t index)
{
  void *target;
 
  if ((int) index > -1) {
    if (!VECTOR_INBOUNDS(index))
      return;
    target = VECTOR_INDEX(index);
  } else {
    if (!VECTOR_HASSPACE(v))
      vector_grow(v, 0);
    target = VECTOR_INDEX(v->num_elems);
    v->num_elems++; /* Only grow when adding a new item not when inserting in a spec indx */
  }
 
  kmemcpy(target, elem, v->elem_size);
}
 
void vector_insert_at(vector *v, void *elem, size_t index)
{
  if ((int) index < 0)
    return;
 
  if (!VECTOR_HASSPACE(v))
    vector_grow(v, 0);
 
  if (index < v->num_elems)
    memmove(VECTOR_INDEX(index + 1), VECTOR_INDEX(index), (v->num_elems - index) * v->elem_size);
 
  /* 1: we are passing index so insert won't increment this 2: insert checks INBONDS... */
  v->num_elems++;
 
  vector_insert(v, elem, index);
}

void vector_push(vector *v, void *elem)
{
  vector_insert(v, elem, -1);
}
 
void vector_pop(vector *v, void *elem)
{
  kmemcpy(elem, VECTOR_INDEX(v->num_elems - 1), v->elem_size);
  v->num_elems--;
}
 
void vector_grow(vector *v, size_t size)
{
  if (size > v->num_alloc_elems)
    v->num_alloc_elems = size;
  else
    v->num_alloc_elems *= 2;
 
  v->elems = krealloc(v->elems, v->elem_size * v->num_alloc_elems);
  //assert(v->elems != NULL);
  if(v->elems == NULL)
    kprintf("something broke allocating vector space");
}

void vector_remove(vector *v, size_t index)
{
  //assert((int) index > 0);
 
  if (!VECTOR_INBOUNDS(index))
    return;
 
  memmove(VECTOR_INDEX(index), VECTOR_INDEX(index + 1), v->elem_size);
  v->num_elems--;
}

void vector_remove_all(vector *v)
{
  v->num_elems = 0;
  v->elems = krealloc(v->elems, v->num_alloc_elems);
  //assert(v->elems != NULL);
}
 
size_t vector_length(vector *v)
{
  return v->num_elems;
}
 
size_t vector_size(vector *v)
{
  return v->num_elems * v->elem_size;
}
 
void vector_get(vector *v, size_t index, void *elem)
{
  //assert((int) index >= 0);
 
  if (!VECTOR_INBOUNDS(index)) {
    elem = NULL;
    return;
  }
 
  kmemcpy(elem, VECTOR_INDEX(index), v->elem_size);
}
 
void vector_cmp_all(vector *v, void *elem, int (*cmp_func)(const void *, const void *))
{
  size_t i;
  void *best_match = VECTOR_INDEX(0);
 
  for (i = 1; i < v->num_elems; i++)
    if (cmp_func(VECTOR_INDEX(i), best_match) > 0)
      best_match = VECTOR_INDEX(i);
 
  kmemcpy(elem, best_match, v->elem_size);
}


