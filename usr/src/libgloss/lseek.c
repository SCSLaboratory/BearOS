/*
 * lseek.c -- move read/write pointer.
 */
#include <sys/types.h>
#include <unistd.h>
#include <sbin/nfsd.h>

/*
 * lseek --  
 */
off_t
_DEFUN (lseek, (file,  offset, whence),
       int file  _AND
       off_t offset _AND
       int whence)
{
  off_t ret;

  ret = nfsd_seek(NFSD,file,(uint64_t)offset,whence);
  return ret;
}



