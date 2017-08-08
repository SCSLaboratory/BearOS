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

/* hremove -- removes and returns an entry under a designated key
 * using a designated search fn -- returns a pointer to the entry or
 * NULL if not found
 */
void *hremove(hashtable_t *htp, 
	      int (*searchfn)(void* elementp, const void* searchkeyp), 
	      const char *key, 
	      int keylen);

/* happly -- applies a function to every entry in hash table */
void happly(hashtable_t *htp, void (*fn)(void* ep));
