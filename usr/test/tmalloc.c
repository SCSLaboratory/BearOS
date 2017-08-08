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
 * tmalloc.c -- malloc test
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/tutils.h>

/*
 * malloc structures and free them
 */
int main(int argc, char *argv[]) {
  void *pp;
  int i,n,datasize;
  
  if(argc!=3 || ((n=atoi(argv[1]))<=0) || ((datasize=atoi(argv[2]))<=0)) {
    printf("Usage: tmalloc <itterations> <problemsize>\n");
    exit(EXIT_FAILURE);
  }

  /* malloc and immediately free, n times */
  for(i=0;i<n; i++)  {
    pp = make_person("somenm",i,datasize); /* make a person */
    if(!check_person(pp,"somenm",i,datasize)) { /* check it */
      printf("[error allocating and freeing: ");
      print_person(pp);
      printf("]\n");
      return(EXIT_FAILURE);      return(EXIT_FAILURE);
    }
    free_person(pp);			/* free it */
  }
  return(EXIT_SUCCESS);
}

