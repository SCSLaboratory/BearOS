/* 
 * unlink.c -- remove a file.
 */
#include "_ansi.h"
#include <sbin/nfsd.h>

/*
 * unlink
 */
int
_DEFUN (unlink, (path),
        char * path)
{
  return nfsd_unlink(NFSD,path);
  //return -1;
}


