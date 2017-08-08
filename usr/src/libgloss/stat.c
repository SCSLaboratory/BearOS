/*
 * stat.c -- Get the status of a file.
 */
#include <_ansi.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sbin/nfsd.h>

/*
 * stat -- Since we have no file system, we just return an error.
 */

int
_DEFUN (stat, (path, buf),
	const char *path _AND
	struct stat *buf)
{
  struct stat *statp;
  return nfsd_stat(NFSD,path,&statp);
}
