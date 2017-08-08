/* open.c -- open a file.
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
#include <sbin/nfsd.h>		/* nfsd_open */
#include <sbin/sysd.h>		/* sysd_fileid */
/*
 * open -- Use libnfs to open a file descriptor on an NFS server.
 */
int
_DEFUN (open, (buf, flags, ...),
       const char *buf _AND
       int flags _AND
       ...)
{
  int ret,mode;
  va_list ap;			/*  arg pointer */

  va_start(ap,flags);		/* start at last named arg */
  mode = va_arg(ap,int);
  va_end(ap);

  ret = -1;
#if ( !defined(STANDALONE) )
  if(strcmp(buf,"stdout")!=0 && strcmp(buf,"stdin")!=0 && strcmp(buf,"stderr")!=0) {
    ret = nfsd_open(NFSD,buf,mode);
  }
#endif
  return ret;
}


