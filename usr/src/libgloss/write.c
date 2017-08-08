/* 
 * write.c -- write bytes to an output
 */
#include <stdio.h>		/* printf */
#include <limits.h>		/* INT_MAX */
#include <sys/socket.h>		/* netd support */
#include <netdb.h>		/* netd support */
#include <redirect.h>
#include <sbin/netd.h>		/* is_sockid */
#include <sbin/vgad.h>		/* Puts_xxx */
#include <sbin/nfsd.h>		/* nfsd_write */
#include <sbin/piped.h>
#include <sbin/daemon_msg_types.h>

/*
 * write -- write bytes to the serial port. Ignore fd, since
 *          stdout and stderr are the same. Since we have no filesystem,
 *          open will only return an error.
 */

static Puts_req_t req;

static inline int writeln(int pid, char *buf, int bytes) {
  Puts_resp_t resp;
  Msg_status_t status;
  int i,ret;

  req.type = VGA_PUTS;
  if(bytes>CHARBUF_SZ)		/* truncate if writing more than 1K */
    bytes=CHARBUF_SZ;
  req.len=bytes;
  for (i = 0; i < bytes; i++,buf++)
    req.buf[i]=*buf;
  msgsend(pid,&req,sizeof(int)+sizeof(uint32_t)+bytes);
  msgrecv(pid,&resp,sizeof(Puts_resp_t),&status);
  ret = bytes;
}

int 
_DEFUN (write, (fd,buf, nbytes),
	int fd _AND
	char *buf _AND
	int nbytes)
{
  int i,ret,cnt,pid;
  char *buffp,*p;
  Msg_status_t status;

  /* write to stdout or stderr */
  ret=-1;			  /* assume failure  */
  if((fd==1) || (fd==2)) {	  /* stdout or stderr */
    /* CANNOT USE sysd_if here */
    pid = getstdio(fd);
    ret=writeln(pid,buf,nbytes);
  }
#if ( !defined(STANDALONE) )
  else if(is_fileid(fd)) {
    ret=nfsd_write(NFSD,buf,(uint64_t)nbytes,fd);
  }
  else if(is_sockid(fd))
    ret=-1;
#endif	/* STANDALONE */
  else if(is_pipeid(fd))
    ret = pipe_write(buf, (uint64_t)nbytes, fd);
  return ret;
}
