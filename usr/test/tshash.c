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

#include <utils/shash.h>
#include <utils/tutils.h>

//#define TSHASH_DEBUG 1

#define MULTIPLE 10		/* #entries = MULTIPLE*tablesize */

#define DATASIZE 10		/* the default size of a persons data */


/*function for testing the ability to pass arguments to the qapply function*/
/*this function counts an element as present if it's not null */
void count_element(void *arg, void *ep){
	 int *count = (int32_t*)arg;
	if(ep!=NULL)
		*count = *count + 1;
} 		



int main(int argc, char *argv[]) {
  person_t *pp;
  int age,key,tablesize;
  shashtable_t *ht;
  void *ep;			/* an entry */
  char nm[NAMESIZE];
  int count =0;

  if(argc!=2 || ((tablesize=atoi(argv[1]))<=0)) {
    printf("[Usage: tshash <tablesize>]\n");
    exit(EXIT_FAILURE);
  }

  /* open a hash table and put MULTIPLE entries in it */
  ht=shopen((uint32_t)tablesize);
  for(key=0;key<(MULTIPLE*tablesize);key++) {
    sprintf(nm,"%s%d","nm",key);
    pp = make_person(nm,key,DATASIZE);
    shput(ht,(void*)pp,key);
  }

#ifdef TSHASH_DEBUG
  printf("\nInitial hashtable:\n");
  shapply(ht,print_person);
#endif
  /* search for and check every value that is known to be present, in
   * the opposite order they were inserted
   */
  for(key=(MULTIPLE*tablesize)-1; key>=0; key--) {
    sprintf(nm,"%s%d","nm",key);
    pp=(person_t*)shsearch(ht,is_age,key);
    check_person(pp,nm,key,DATASIZE);
  }
#ifdef TSHASH_DEBUG
  printf("[hashtable search succeeded]\n");
#endif

  /* search for something thats not there */
  key=(MULTIPLE*tablesize);	/* one more than biggest key used */
  if(shsearch(ht,is_age,key)!=NULL) {
#ifdef TSHASH_DEBUG
    printf("[error: absent entry found]\n");
#endif
    exit(EXIT_FAILURE);
  }
#ifdef TSHASH_DEBUG
  printf("[search for non-exitant entry succeeded]\n");
#endif

	/*get the current size of the hash table */
	shapply2(ht, count_element, &count);
#ifdef TSHASH_DEBUG
	printf("hash count is %d", count);
#endif
  /* remove each entry and make sure whats removed is correct */
  for(key=(MULTIPLE*tablesize)-1; key>=0; key--) {
    sprintf(nm,"%s%d","nm",key);
    pp=(person_t*)shremove(ht,is_age,key);
    check_person(pp,nm,key,DATASIZE);
    free_person(pp);
  }
#ifdef TSHASH_DEBUG
  printf("[removing all entries succeeded]\n");
#endif

#ifdef TSHASH_DEBUG
  printf("Final Hashtable:\n");
  shapply(ht,print_person);
#endif

  /* close the hash table and terminate */
  shclose(ht);

  return(EXIT_SUCCESS);
}

