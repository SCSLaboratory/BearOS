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
 * ls.c -- List contents of a directory
 * Usage ls [-l] <filenm>
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <string.h>		/* strsep */
#include <inttypes.h>		/* PRId64 */
#include <utils/bool.h>		/* TRUE/FALSE */

#include <sbin/nfsd.h>		/* nfsd interface */
#include <sbin/sysd.h>		/* sysd interface */

/* The nfsd Message Buffer */

static int print_dirent(int longform,char *dir,char *nm);

int main(int argc, char *argv[])
{
  char apath[MAXPATHLEN],dirfilenm[MAXPATHLEN];
  int  longform, dirh, res;

  if(argc<1 || argc>3) {
    printf("Usage: ls [-l] <filename>");
    exit(EXIT_FAILURE);
  }

  /* parse the command line argments to produce a fullpath */
  abspath(".",apath);
  longform=FALSE;
  if(argc!=1) {			  /* not just ls */
    if(strcmp(argv[1],"-l")==0) { /* ls -l */
      longform=TRUE;
      if(argc==3)		/* ls -l <filename> */
	abspath(argv[2],apath);
    }
    else			/* ls <filename> */
      abspath(argv[1],apath);
  }
  /* open the directory */
  if((dirh=nfsd_opendir(NFSD,apath))<0) {
    printf("%s: no such directory\n",apath);
    exit(EXIT_FAILURE);
  }
  /* get the directory entries one at a time and print them */
  do {
    res=FALSE;
    /* set dirfilenm to next directory entry */
    if(nfsd_readdir(NFSD,dirh,dirfilenm)==DACK)
      res=print_dirent(longform,apath,dirfilenm);
  }
  while(res);
  /* close up */
  if(nfsd_closedir(NFSD,dirh)<0) {
    printf("%s: unable to close\n",apath);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

/* 
 * print a single directory entry 
 */
static int print_dirent(int longform,char *dir,char *nm) {
  char finalpath[MAXPATHLEN], filepath[MAXPATHLEN];
  struct stat *statp;
  int ret;

  if((strcmp(nm,"..")==0) || (strcmp(nm,".")==0))
     return TRUE;

  strncpy(finalpath,dir,MAXPATHLEN);
  strncat(finalpath,"/",MAXPATHLEN);
  strncat(finalpath,nm,MAXPATHLEN);
  abspath(finalpath,filepath); /* check and correct it */
  
  if((ret=nfsd_stat(NFSD,filepath,&statp))<0) {
    /* grab the status entry out of the reply */
    printf("%s: unable to access entry (%d)\n",filepath,ret);
    return FALSE;
  }
  if(longform) {
    switch ((statp->st_mode) & S_IFMT) {
    case S_IFREG: printf("-"); break; /* regular file */
    case S_IFDIR: printf("d"); break; /* directory */
    case S_IFCHR: printf("c"); break; /* character special file */
    case S_IFBLK: printf("b"); break; /* block special file */
    case S_IFLNK: printf("l"); break; /* symbolic link */
    case S_IFSOCK: printf("s"); break; /* socket link */
    case S_IFIFO: printf("p"); break; /* fifo link */
    case S_IFMT: printf("m"); break;  /* mount point maybe? */
    default: printf("?"); break;      /* who the hell knows! */
    }
    printf("%c%c%c",
	   "-r"[!!((statp->st_mode) & S_IRUSR)],
	   "-w"[!!((statp->st_mode) & S_IWUSR)],
	   "-x"[!!((statp->st_mode) & S_IXUSR)]);
    printf("%c%c%c",
	   "-r"[!!((statp->st_mode) & S_IRGRP)],
	   "-w"[!!((statp->st_mode) & S_IWGRP)],
	   "-x"[!!((statp->st_mode) & S_IXGRP)]);
    printf("%c%c%c",
	   "-r"[!!((statp->st_mode) & S_IROTH)],
	   "-w"[!!((statp->st_mode) & S_IWOTH)],
	   "-x"[!!((statp->st_mode) & S_IXOTH)]);
    printf(" %3d", (int)statp->st_nlink);
    //    printf(" %5d", (int)statp->st_uid);
    // printf(" %5d", (int)statp->st_gid);
    printf(" %12" PRId64, statp->st_size);
    printf(" ");
  }
  printf("%s\n", nm);
  return TRUE;
}


