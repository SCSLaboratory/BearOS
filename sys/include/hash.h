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

typedef struct hash_table {
	uint64_t table_size;	
	void **table;		/* pointer to a table of void* */
} hash_ctx;

/* hopen -- opens a hash table of size hsize */
hash_ctx *hopen(uint32_t hsize);

/* hclose -- closes a hash table */
void hclose(hash_ctx *vhtp);

/* hput -- puts an entry into a hash table under designated key */
void hput(hash_ctx *htp, void *ep, const char *key, int keylen);

/* hsearch -- searchs for an entry under a designated key using a designated search fn */
void *hsearch(hash_ctx *htp, int (*searchfn)(void* elementp, void* searchkeyp), const char *key, int keylen);


/* hsearch -- removes and returns an entry under a designated key using a designated search fn */
void *hremove(hash_ctx *htp, int (*searchfn)(void* elementp, void* searchkeyp), const char *key, int keylen);

/* happly -- applies a function to every entry in hash table */
void happly(hash_ctx *htp, void (*fn)(void* ep));
