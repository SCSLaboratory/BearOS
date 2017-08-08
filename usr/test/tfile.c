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
 * tfile.c -- Newlib file operations
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <newlib.h>
#include <stdint.h>
#include <fcntl.h>

#define TFILE_SHOW 1          /* set this to show progress and errors */

/* local helper functions */
static void errorexit(char *errorstr);
static void clear_data(void *buffp,int sz);
static void set_data(void *buffp,int sz);
static void check_data(void *buffp,int start,int sz);

#define BLOCKSZ (1024*sizeof(int))	   /* read 1k integer blocks */
char block[BLOCKSZ];

#define ROUNDS 5

int main(int argc, char *argv[]) {
  int res;			/* daemon pid, return code */
  int i,filesize,bytesread; 
  char filenm[MAXPATHLEN];
  void *buffp;
  struct stat st;
  FILE *fh;
  int posn;

  if(argc!=1)
    errorexit("Usage: tfile - tests stdio file operations\n");

  /*
   * FILE operation checks: create, write, read, and stat 5 files
   * lengths vary: 100,800,6400,51200,409600 bytes 
   */
  for(filesize=100,i=0; i<ROUNDS; i++) {
    /* create the file name: bar0,bar1,... bar4 */
    sprintf(filenm,"%s%d","/bar",i); 

    /* create a buffer of data - integers 0,1,2...(filesize/sizeof(int)) */
    buffp = malloc(filesize);
    set_data(buffp,filesize);	
    
    if(stat(filenm,&st)==0)
      errorexit("stat error: file exists\n");

    /* open for writing */
    if((fh=fopen(filenm,"w"))<0)
      errorexit("unable to open file for writing\n");
    /* write the data */
    if(fwrite(buffp,sizeof(char),filesize,fh)<0) 
      errorexit("unable to write file\n");
    /* close the file */
    if(fclose(fh)<0) 
      errorexit("unable to close file\n");

    if(stat(filenm,&st)!=0)
      errorexit("stat error: file does not exist\n");
    /* clear the buffer */
    clear_data(buffp,filesize);

    /* reopen the file for reading */
    if((fh=fopen(filenm,"r"))<0) 
      errorexit("unable to open file for reading\n");
    /* read the data */
    if((bytesread=fread(buffp,sizeof(char),filesize,fh))<0)
      errorexit("unable to read file\n");
    /* close the file */
    if(fclose(fh)<0) 
      errorexit("unable to close file\n");

    /* check its correct */
    check_data(buffp,0,filesize);
    /* done with this file */

    if(i<ROUNDS-1) {
      if(stat(filenm,&st)!=0)
	errorexit("stat error: file does not exit\n");
      /* remove the file */
      if(remove(filenm)!=0) 
	errorexit("unable to remove file");
      /* check its gone */
      if(stat(filenm,&st)==0)
	errorexit("stat error: removed file exists\n");
      filesize *= 8;
    }
    free(buffp);
  }

  if(ROUNDS>0 && filesize > 6*BLOCKSZ) {
    /* 
     * SEEK checks -- seek through 1 Mbyte file (foo3)
     */
    sprintf(filenm,"%s%d","/bar",ROUNDS-1); 
    /* reopen the file for reading */
    if((fh=fopen(filenm,"r"))<0) 
      errorexit("unable to open bar4 for seeking\n");
    
    /* seek over 1K ints */
    if(fseek(fh,BLOCKSZ,SEEK_SET)<0)
      errorexit("[Unable to set seek point in bar4]\n");
    posn=(int)ftell(fh);
    if(posn!=BLOCKSZ) {
      printf("bad seek (1): expected: %d, posn: %d\n",(int)BLOCKSZ,posn);
      exit(EXIT_FAILURE);
    }
    /* read a block of integers */
    if((bytesread=fread(block,sizeof(char),BLOCKSZ,fh))<0)
      errorexit("[Unable to read first 4kbytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize]\n");
    check_data(block,(BLOCKSZ/sizeof(int)),BLOCKSZ); /* check its right */
    
    /* SEEK_CUR check: seek one block forward (1024 integers) */
    if(fseek(fh,BLOCKSZ,SEEK_CUR)<0)
      errorexit("[Unable to seek forward in bar4]\n");
    posn=(int)ftell(fh);
    if(posn!=(3*BLOCKSZ)) {
      printf("bad seek (2): expected: %d, posn: %d\n",(int)(3*BLOCKSZ),posn);
      exit(EXIT_FAILURE);
    }
    if((bytesread=fread(block,sizeof(char),BLOCKSZ,fh))<0)
      errorexit("[Unable to read second 4kbytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize after second seek]\n");
    /* check the data */
    check_data(block,((3*BLOCKSZ)/sizeof(int)),BLOCKSZ);	/* check its right */
    
    /* do it again */
    if(fseek(fh,BLOCKSZ,SEEK_CUR)<0)
      errorexit("[Unable to seek forward again in bar4]\n");
    posn=(int)ftell(fh);
    if(posn!=(5*BLOCKSZ)) {
      printf("bad seek (3): expected: %d, posn: %d\n",(int)(5*BLOCKSZ),posn);
      exit(EXIT_FAILURE);
    }
    if((bytesread=fread(block,sizeof(char),BLOCKSZ,fh))<0)
      errorexit("[Unable to read third 4kbytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize after third seek]\n");
    /* check the data */
    check_data(block,((5*BLOCKSZ)/sizeof(int)),BLOCKSZ);	/* check its right */
    
    /* SEEK_END check: check we can seek to the end of the file */
    if(fseek(fh,-(sizeof(int)),SEEK_END)<0) 
      errorexit("[Unable to set seek to end of file]\n");
    posn=(int)ftell(fh);
    if(posn!=(filesize-sizeof(int))) {
      printf("bad seek (4): expected: %d, posn: %d\n",(int)(filesize-sizeof(int)),posn);
      exit(EXIT_FAILURE);
    }
    /* read the last integer in the file */
    if((bytesread=fread(block,sizeof(char),sizeof(int),fh))!=sizeof(int))
      errorexit("[Unable to read in last integer]\n");
    /* check last integer = */
    if(*((int*)block)!=(filesize/sizeof(int))-1)
      errorexit("[Last integer in error]\n");
    
    if(fclose(fh)<0) 
      errorexit("unable to close file\n");
    
    /* remove the file */
    if(remove(filenm)!=0) 
      errorexit("unable to remove file");
    /* check its gone */
    if(stat(filenm,&st)==0)
      errorexit("stat error: removed file exists\n");
  }
  exit(EXIT_SUCCESS);
}



/* helper routines */

static void errorexit(char *errorstr) {
#ifdef TFILE_SHOW
  printf("%s",errorstr);
#endif
  exit(EXIT_FAILURE);
}

/*
 * set_data -- sets a data buffer to integers in increasing order
 */
static void set_data(void *buffp,int numbytes) {
  int *p;
  int i;
  p=(int*)buffp;

  for(i=0;i<(numbytes/sizeof(int));i++,p++)
    *p = i;
}

/*
 * check_data - check data read into a buffer based on a starting value
 */
static void check_data(void *buffp,int start,int sz) {
  int *p;
  int i;

  p=(int*)buffp;
  for(i=0;i<(sz/sizeof(int));i++,p++)
    if(*p != (start+i)) {
#ifdef TFILE_SHOW
      printf("[invalid data recieved\n");
#endif
      exit(EXIT_FAILURE);
    }
}

/* 
 * clear_data -- sets the data to 0
 */
static void clear_data(void *p,int numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *((char*)p) = 0;
}
