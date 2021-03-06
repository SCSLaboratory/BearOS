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
 * touch.c -- touch a file
*/
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <string.h>		/* strsep */
#include <sbin/nfsd.h>		/* nfsd interface */

int main(int argc, char *argv[]) {
  char filenm[MAXPATHLEN];
  int fh;
  struct stat *statp;
  
  if(argc!=2) {
    printf("Usage: touch <file>\n");
    exit(EXIT_FAILURE);
  }

  abspath(argv[1],filenm);

  /* make sure 'filenm' does not exist */
  if(nfsd_stat(NFSD,filenm,&statp)==0) {
    printf("%s: already exists\n",filenm);
    exit(EXIT_FAILURE);
  }
  /* create it */
  if((fh=nfsd_open(NFSD,filenm,O_WRONLY))<0)  {
    printf("%s: unable to create file\n",filenm);
    exit(EXIT_FAILURE);
  }
  /* close it with zero length */
  if(nfsd_close(NFSD,fh)<0) {
    printf("%s: unable to close file\n",filenm);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
