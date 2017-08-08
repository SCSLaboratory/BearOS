/* 
 * read.c 
 */
#include <stdio.h>
#include <limits.h>
#include <sys/socket.h>
#include <netdb.h>
#include <redirect.h>
#include <sbin/kbd.h>
#include <sbin/netd.h>
#include <sbin/nfsd.h>		/* nfsd_read(...) */
#include <sbin/piped.h>

/*
 * read  
 */
int
_DEFUN (read, (fd, buf, nbytes),
       int fd _AND
       char *buf _AND
       int nbytes)
{
  int ret;

  /* apply any redirection */
  fd = apply_redirect(fd);

  ret=0;
  if(fd==0)
    ret = getstr(buf,nbytes);

#if ( !defined(STANDALONE) )
  else if(is_fileid(fd))
    ret=nfsd_read(NFSD,buf,(uint64_t)nbytes,fd);
  else if(is_sockid(fd))
    ret=-1;			/* error */
#endif

  else if ( is_pipeid(fd) )
    ret = pipe_read(buf, (uint64_t)nbytes, fd);
  return ret;
}
