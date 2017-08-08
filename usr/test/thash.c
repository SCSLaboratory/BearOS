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
/*
 * thash.c -- regression test for the hash module
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/hash.h>
#include <utils/tutils.h>

//#define THASH_DEBUG 1

#define DATASIZE 10		/* the default size of a persons data */

#define MULTIPLE 10		/* #entries = 10*tablesize */

int main(int argc, char *argv[]) {
  void *pp;
  int age,key,tablesize;
  hashtable_t *ht;
  void *ep;			/* an entry */
  char nm[NAMESIZE];

  if(argc!=2 || ((tablesize=atoi(argv[1]))<=0)) {
    printf("[Usage: thash <tablesize>]\n");
    exit(EXIT_FAILURE);
  }

  /* open a hash table and put MULTIPLE entries in it */
  ht=hopen((uint32_t)tablesize);
  for(key=0;key<(MULTIPLE*tablesize);key++) {
    sprintf(nm,"%s%d","nm",key);
    pp = make_person(nm,key,DATASIZE);
    hput(ht,(void*)pp,(char*)&key,sizeof(key));
  }

#ifdef THASH_DEBUG
  printf("\nInitial RANDOM hashtable:\n");
  happly(ht,print_person);
#endif
  /* search for and check every value that is known to be present, in
   * the opposite order they were inserted
   */
  for(key=(MULTIPLE*tablesize)-1; key>=0; key--) {
    sprintf(nm,"%s%d","nm",key);
    pp=(person_t*)hsearch(ht,is_age,(char*)&key,sizeof(key));
    check_person(pp,nm,key,DATASIZE);
  }
#ifdef THASH_DEBUG
  printf("[hashtable search succeeded]\n");
#endif

  /* search for something thats not there */
  key=(MULTIPLE*tablesize);	/* one more than biggest key used */
  if(hsearch(ht,is_age,(char*)&key,sizeof(key))!=NULL) {
#ifdef THASH_DEBUG
    printf("[error: absent entry found]\n");
#endif
    exit(EXIT_FAILURE);
  }
#ifdef THASH_DEBUG
  printf("[search for non-exitant entry succeeded]\n");
#endif


  /* remove each entry and make sure whats removed is correct */
  for(key=(MULTIPLE*tablesize)-1; key>=0; key--) {
    sprintf(nm,"%s%d","nm",key);
    pp=(person_t*)hremove(ht,is_age,(char*)&key,sizeof(key));
    check_person(pp,nm,key,DATASIZE);
    free_person(pp);
  }
#ifdef THASH_DEBUG
  printf("[removing all entries succeeded]\n");
#endif

#ifdef THASH_DEBUG
  printf("Final Hashtable:\n");
  happly(ht,print_person);
#endif

  /* close the hash table and terminate */
  hclose(ht);
  return(EXIT_SUCCESS);
}

