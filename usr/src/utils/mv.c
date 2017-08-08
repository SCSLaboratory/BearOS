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
 * mv.c -- removes a file or directory
*/
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <string.h>		/* strsep */
#include <sbin/nfsd.h>		/* nfsd interface */

int main(int argc, char *argv[]) {
  char from[MAXPATHLEN], to[MAXPATHLEN], new[MAXPATHLEN];
  struct stat *statp;

  if(argc!=3) {
    printf("Usage: mv <from> <to>\n");
    exit(EXIT_FAILURE);
  }

  abspath(argv[2],new);		/* grab abs path for dest */

  /* stat 'to' */
  if(nfsd_stat(NFSD,new,&statp)==0) { /* already exists? */
    if(((statp->st_mode) & S_IFMT)==S_IFDIR) { /* directory? */
      if(*argv[1]!='/' && *argv[1]!='~') {     /* src just a file name? */
	strcat(new,"/");		       /* add a separator */
	strcat(new,argv[1]);		       /* concat filenm onto dest */
	abspath(new,to);		       /* strip extra slashes */
	if(nfsd_stat(NFSD,to,&statp)==0) {       /* already exists */
	  printf("%s: already exists\n",to);
	  exit(EXIT_FAILURE);
	} /* to now contains an ok dest */
      }
      abspath(argv[1],from);	/* use from as is */
    }
    else { 			/* otherwise its a file */
      printf("%s: file exists\n",to);
      exit(EXIT_FAILURE);
    }
  }
  else {			/* to does not exist */
    abspath(argv[1],from);	/* use as is */
    abspath(argv[2],to);
  }
  if(nfsd_stat(NFSD,from,&statp)!=0) { 
    printf("%s: no such file\n",from);
    exit(EXIT_FAILURE);
  }
  /* to and from both defined */
  if(nfsd_rename(NFSD,from,to)!=0) { /* do the move */
    printf("%s: unable to move file\n",from);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
