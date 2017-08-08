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
 * mkdir.c -- make a directory.
 *
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <string.h>		/* strsep */
#include <sbin/nfsd.h>		/* nfsd interface */

/* 
 * mkdir: make a directory
 */
int main(int argc, char *argv[]) {
  char path[MAXPATHLEN];

  if(argc==2)
    abspath(argv[1],path);
  else if(argc==3 && strcmp(argv[1],"-s")==0) {
    abspath(argv[2],path);
    printf("[removing %s]\n",path);    
  }
  else {
    printf("Usage: rmdir [-s] <dirname>\n");
    exit(EXIT_FAILURE);
  }
  /* make the directory */
  if(nfsd_rmdir(NFSD,path)<0) {
    printf("%s: unable to remove directory\n",path);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
