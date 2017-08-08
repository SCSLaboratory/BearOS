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
 * cp.c -- copies a file
*/
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <string.h>		/* strsep */
#include <sbin/nfsd.h>		/* nfsd interface */

int main(int argc, char *argv[]) {
  char from[MAXPATHLEN], to[MAXPATHLEN];
  int rdfh,wrfh,bytesread;
  char buff[MAXBUFF];
  struct stat *statp;

  if(argc!=3) {
    printf("Usage: cp <fromfile> <tofile>\n");
    exit(EXIT_FAILURE);
  }

  abspath(argv[1],from);
  abspath(argv[2],to);

  /* make sure 'from' exists */
  if(nfsd_stat(NFSD,from,&statp)<0) {
    printf("%s: no such file\n",from);
    exit(EXIT_FAILURE);
  }
  /* make sure 'to' does not exist */
  if(nfsd_stat(NFSD,to,&statp)==0) {
    printf("%s: already exists\n",to);
    exit(EXIT_FAILURE);
  }

  /* open the files */
  if((rdfh=nfsd_open(NFSD,from,O_RDONLY))<0) {
    printf("%s: unable to open file\n",from);
    exit(EXIT_FAILURE);
  }
  if((wrfh=nfsd_open(NFSD,to,O_WRONLY))<0)  {
    printf("%s: unable to create file\n",to);
    exit(EXIT_FAILURE);
  }
  /* copy the file */
  do {
    if((bytesread=nfsd_read(NFSD,buff,MAXBUFF,rdfh))>=0) {
      /* write the data out */
      if(bytesread>0 && nfsd_write(NFSD,buff,bytesread,wrfh)<0) {
	printf("%s: error writing file\n",to);
	exit(EXIT_FAILURE);
      }
    }
    else {
      printf("%s: error reading file\n",from);
      exit(EXIT_FAILURE);
    }
  } while(bytesread>0);

  /* close the files */
  if(nfsd_close(NFSD,rdfh)<0) {
    printf("%s: unable to close file\n",from);
    exit(EXIT_FAILURE);
  }
  if(nfsd_close(NFSD,wrfh)<0) {
    printf("%s: unable to close file\n",to);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
