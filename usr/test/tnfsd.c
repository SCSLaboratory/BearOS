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
 * tnfsd.c -- The nfsd regression test
 *
 * Each Daemon has an associated test.  Eventually, every daemon will
 * have its own log; for the time being, only enable printing from
 * either the test or the daemon, but not both. Each has its own DEBUG
 * flag to allow you to choose which ONE will do printing. They both
 * use the same printing routine for messages in nfsd_utils.c.
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <sys/wait.h>		/* waitpid */
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>
#include <fcntl.h>

//#include <libnfs.h>
#include <sbin/nfsd.h>		/* nfsd interface */
#include <sbin/daemon_msg_types.h>

#define MAXLINE 128
#define MAXARGS 20
#define OVERWRITE 1
#define TRUE 1
#define FALSE 0
#define ROUNDS 4		/* do ROUNDS pings as a test */
#define PINGDELAY 1		/* seconds between pings */

#define TNFSD_SHOW 1          /* set this to show progress and errors */

/* local helper functions */
static void errorexit(char *errorstr);
static void set_data(void *buffp,uint64_t sz);
static void clear_data(void *buffp,uint64_t sz);
static void check_data(void *buffp,int startpoint,uint64_t sz);

#define BLOCKSZ (1024*sizeof(int))	   /* read 1k integer blocks */
char block[BLOCKSZ];


/* 
 * tnfsd -- Regression test for a nfsd.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */

int main(int argc, char *argv[]) {
  int len,j,k,dpid,res,cnt;			/* daemon pid, return code */
  int wstatus,i,dfh; 
  char filenm[MAXPATHLEN];
  char *buffp,*p, *datap;
  uint64_t blockpsn;
  uint64_t filesize,bytesread,byteswrote;
  int posn;
  struct stat *statp;

  if(argc==1)
    dpid=NFSD;			/* initialized by shell */
  else if(argc==2) 
    dpid = atoi(argv[1]);
  else
    errorexit("Usage: tnfsd [nfspid]\n");

  /* STAT a non-existant file */
  if((res=nfsd_stat(dpid,"ranDom-NonexisTant-fIle",&statp))>=0)
    errorexit("[Stat on non-existant file succeeded!]\n");

  /*
   * FILE operation checks: create, write, read, and stat 4 files
   * lengths vary: 0.5K,4K,32K,256K
   */
  for(filesize=512,i=0; i<ROUNDS; i++) {
    /* create the file name: foo0,foo1,... foo3 */
    datap = (char*)malloc(filesize);
    set_data(datap,filesize);	
    sprintf(filenm,"%s%d","/foo",i); 

    /* stat the file to make sure it doesnt exist */
    if(nfsd_stat(dpid,filenm,&statp)==0) 
      errorexit("[Stat file exists]\n");

    /* open for writing */
    if((dfh=nfsd_open(dpid,filenm,O_WRONLY))<0) 
      errorexit("[Unable to open file for writing]\n");
    /* write the file */
    if((byteswrote=nfsd_write(dpid,datap,filesize,dfh))<0)
      errorexit("[Unable to write file]\n");
    /* close the file */
    if(nfsd_close(dpid,dfh)<0) 
      errorexit("[Unable to close file]\n");

    /* stat the file to make sure it exists */
    if(nfsd_stat(dpid,filenm,&statp)<0) 
      errorexit("[Unable to stat]\n");
    /* clear out the data */
    clear_data(datap,filesize);


    /* reopen the file for reading */
    if((dfh=nfsd_open(dpid,filenm,O_RDONLY))<0) 
      errorexit("[Unable to open file for reading]\n");
    /* read in the file */
    if((bytesread=nfsd_read(dpid,datap,filesize,dfh))<0)
      errorexit("[Unable to read file]\n");
    /* check what was read is ok */
    check_data(datap,0,filesize);
    free(datap);
    /* close the file */
    if(nfsd_close(dpid,dfh)<0) 
      errorexit("[Unable to close file]\n");

    /* on the first 3 files */
    if(i<ROUNDS-1) {
      /* remove the file */
      if(nfsd_unlink(dpid,filenm)<0)
	errorexit("[Unable to remove file]\n");
      /* check it was removed */
      if(nfsd_stat(dpid,filenm,&statp)>=0)
	errorexit("[Stat on removed file succeeded!]\n");
      filesize *= 8;		/* increase the file size */
    }
  }

  if(ROUNDS>0 && filesize > 6*BLOCKSZ) {
    /* 
     * SEEK checks -- seek through 1 Mbyte file (foo3)
     */
    sprintf(filenm,"%s%d","/foo",ROUNDS-1); 
    if((dfh=nfsd_open(dpid,filenm,O_RDONLY))<0) 
      errorexit("[Unable to open foo3 for seeking]\n");
    
    /* SEEK_SET check */
    blockpsn=1024*sizeof(int);	   /* start 1k integers in  */
    /* start at index 1024 */

    if((posn=nfsd_seek(dpid,dfh,blockpsn,SEEK_SET))<0) /* seek to offset 4096 bytes*/
      errorexit("[Unable to set seek point in foo3]\n");
    if(posn!=BLOCKSZ) {
      printf("bad seek (1): expected: %d, posn: %d\n",(int)BLOCKSZ,(int)posn);
      exit(EXIT_FAILURE);
    }

    /* read a block of integers */
    if((bytesread=nfsd_read(dpid,block,BLOCKSZ,dfh))<0)
      errorexit("[Unable to read first 4kbytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize]\n");
    check_data(block,1024,BLOCKSZ); /* check its right */
  
    /* SEEK_CUR check: seek one block forward (1024 integers) */
    if((posn=nfsd_seek(dpid,dfh,BLOCKSZ,SEEK_CUR))<0) 
      errorexit("[Unable to set seek forward 4kbytes]\n");
    if(posn!=(3*BLOCKSZ)) {
      printf("bad seek (2): expected: %d, posn: %d\n",(int)BLOCKSZ,(int)posn);
      exit(EXIT_FAILURE);
    }
    /* read a block starting at 3rd block */
    if((bytesread=nfsd_read(dpid,block,BLOCKSZ,dfh))<0) 
      errorexit("[Unable to read second 4k bytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize]\n");
    /* check the data */
    check_data(block,3072,BLOCKSZ);	/* check its right */

    /* SEEK_CUR check: seek one block forward (1024 integers) */
    if((posn=nfsd_seek(dpid,dfh,BLOCKSZ,SEEK_CUR))<0) 
      errorexit("[Unable to set seek forward 4kbytes]\n");
    if(posn!=(5*BLOCKSZ)) {
      printf("bad seek (3): expected: %d, posn: %d\n",(int)BLOCKSZ,(int)posn);
      exit(EXIT_FAILURE);
    }
    /* read a block starting at 3rd block */
    if((bytesread=nfsd_read(dpid,block,BLOCKSZ,dfh))<0) 
      errorexit("[Unable to read second 4k bytes]\n");
    if(bytesread!=BLOCKSZ)
      errorexit("[Wrong blocksize]\n");
    /* check the data */
    check_data(block,5120,BLOCKSZ);	/* check its right */
    
    /* SEEK_END check: check we can seek to the end of the file */
    if((posn=nfsd_seek(dpid,dfh,-(sizeof(int)),SEEK_END))<0) 
      errorexit("[Unable to set seek to end of file]\n");
    if(posn!=filesize-sizeof(int)) {
      printf("bad seek (4): expected: %d, posn: %d\n",(int)(filesize-sizeof(int)),(int)posn);
      exit(EXIT_FAILURE);
    }
    /* read the last integer in the file */
    if((bytesread=nfsd_read(dpid,block,sizeof(int),dfh))!=sizeof(int))
      errorexit("[Unable to read in last integer]\n");
    if(bytesread!=sizeof(int))
      errorexit("[Wrong blocksize]\n");
    /* check last integer = */
    if(*((int*)block)!=(filesize/sizeof(int))-1)
      errorexit("[Last integer in error]\n");
    
    /* close the file */
    if(nfsd_close(dpid,dfh)<0) 
      errorexit("[Unable to close foo3]\n");
    
    /* remove foo3 */
    if(nfsd_unlink(dpid,filenm)<0)
      errorexit("[Unable to remove file]\n");
    /* check to make sure it was removed */
    if(nfsd_stat(dpid,filenm,&statp)>=0)
      errorexit("[Stat on removed file succeeded!]\n");
  }
  exit(EXIT_SUCCESS);
}


/* helper routines */

static void errorexit(char *errorstr) {
#ifdef TNFSD_SHOW
  printf("%s",errorstr);
#endif
  exit(EXIT_FAILURE);
}

/*
 * set_data -- sets a data buffer to integers in increasing order
 */
static void set_data(void *buffp,uint64_t numbytes) {
  int *p;
  int i;
  p=(int*)buffp;

  for(i=0;i<(numbytes/sizeof(int));i++,p++)
    *p = i;
}

/*
 * clear_data -- sets a data buffer to 0
 */
static void clear_data(void *buffp,uint64_t numbytes) {
  int *p;
  int i;
  p=(int*)buffp;

  for(i=0;i<(numbytes/sizeof(int));i++,p++)
    *p = 0;
}

/*
 * check_data - check data read into a buffer based on a start point
 * in the file, the number of bytes requested, and the number of bytes
 * read into the buffer -- assumes data was written with set_data.
 */
static void check_data(void *buffp,int start,uint64_t numbytes) {
  int *p;
  int i;

  p=(int*)buffp;

  for(i=0;i<(numbytes/sizeof(int));i++,p++)
    if(*p != (start+i)) {
#ifdef TNFSD_SHOW
      printf("invalid data recieved\n");
#endif
      exit(EXIT_FAILURE);
    }
}


