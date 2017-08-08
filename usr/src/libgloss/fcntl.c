/* fcntl.c -- open a file.
 * 
 * Copyright (c) 1995 Cygnus Support
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */
#include <stdarg.h>
#include <fcntl.h>
#include <sbin/nfsd.h>

/*
 * open -- Use libnfs to open a file descriptor on an NFS server.
 */
int
_DEFUN (fcntl, (fd, cmd, ...),
	int fd _AND
	int cmd _AND
	...)
{
  int ret;
  int val;
  va_list ap;			/*  arg pointer */

  val=0;
  switch(cmd) {
  case F_GETFD:
  case F_GETFL:
    break;
  case F_SETFD:
  case F_SETFL:
    va_start(ap,cmd);		/* start at last named arg */
    val = va_arg(ap,int);
    va_end(ap);
    break;
  default:
    printf("fcntl: cmd=%d not implmented\n",cmd);
    break;
  }
  ret = nfsd_fcntl(NFSD,fd,cmd,val);
  return ret;
}


