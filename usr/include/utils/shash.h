#pragma once

/*
 * shash.h -- A sequential hash table implementation. 
 *
 * Keys are positive integers and are allocated SEQUENTIALLY from 1;
 * if more general keys are needed, use the generic hash table
 * inplementation <hash.h>.
 */

typedef void shashtable_t;	/* representation of a hashtable hidden */

/* shopen -- opens a hash table of size hsize */
shashtable_t *shopen(uint32_t hsize);

/* shclose -- closes a hash table */
void shclose(shashtable_t *htp);

/* shput -- puts an entry into a hash table under designated key */
void shput(shashtable_t *htp, void *ep, int key);

/* shsearch -- searchs for an entry under a designated key using a
 * designated search function (sfn)-- returns a pointer to the entry
 * or NULL if not found
 */
void *shsearch(shashtable_t *htp,int (*sfn)(void* ep,const void *skey),int key);

/* shsearch -- removes and returns an entry under a designated key
 * using a designated search fn -- returns a pointer to the entry or
 * NULL if not found
 */
void *shremove(shashtable_t *htp,int (*sfn)(void* ep,const void *skey),int key);

/* shapply -- applies a function to every entry in hash table */
void shapply(shashtable_t *htp, void (*fn)(void* ep));

void shapply2(shashtable_t *htp, void (*fn)(void *arg, void *ep), int *arg);
