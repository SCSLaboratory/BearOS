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

/*
 * hash.h -- A generic hash table implementation, allowing arbitrary
 * key structures.
 *
 * NOTE: that if keys are positive integers allocated SEQUENTIALLY
 * from 1, use the sequenced hash table <shash.h> -- it is simpler and
 * more efficient for this special, but particularly useful case.
 */

typedef void hashtable_t;	/* representation of a hashtable hidden */

/* hopen -- opens a hash table of size hsize */
hashtable_t *hopen(uint32_t hsize);

/* hclose -- closes a hash table */
void hclose(hashtable_t *htp);

/* hput -- puts an entry into a hash table under designated key */
void hput(hashtable_t *htp, void *ep, const char *key, int keylen);

/* hsearch -- searchs for an entry under a designated key using a
 * designated search fn -- returns a pointer to the entry or NULL if
 * not found
 */
void *hsearch(hashtable_t *htp, 
	      int (*searchfn)(void* elementp, const void* searchkeyp), 
	      const char *key, 
	      int keylen);

/* hsearch -- removes and returns an entry under a designated key
 * using a designated search fn -- returns a pointer to the entry or
 * NULL if not found
 */
void *hremove(hashtable_t *htp, 
	      int (*searchfn)(void* elementp, const void* searchkeyp), 
	      const char *key, 
	      int keylen);

/* happly -- applies a function to every entry in hash table */
void happly(hashtable_t *htp, void (*fn)(void* ep));

void happly2(hashtable_t *htp, void *arg, void (*fn)(void *arg, void *ep));
