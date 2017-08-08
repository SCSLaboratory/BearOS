/* close.c -- close a file descriptor.
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
#include <stdio.h>
#include <limits.h>
#include <sys/socket.h>
#include <netdb.h>
#include <redirect.h>
#include <sbin/netd.h>
#include <sbin/nfsd.h>		/* nfsd_close */
#include <sbin/sysd.h>		/* is_fileid */
#include <sbin/piped.h>

/*
 * close -- Use libnfs to close the file on the NFS server 
 */
int
_DEFUN (close ,(fd), 
	int fd)
{
  Net_close_req_t close_req;
  Net_close_resp_t close_resp;
  Msg_status_t status;

  int ret;

  /* apply any redirection */
  fd = apply_redirect(fd);

  ret=EOF;
  if(is_stdioid(fd)) 		/* need to close the file at sysd */
      return ret;
#if ( !defined(STANDALONE) )
  else if(is_fileid(fd)) 
    ret=nfsd_close(NFSD,fd);	/* close the file */
#endif
#ifndef STANDALONE
  else if(is_sockid(fd)) {
    close_req.type = NET_CLOSE;
    close_req.sockfd = fd;
    msgsend(NETD,&close_req,sizeof(close_req));
    msgrecv(NETD,&close_resp,sizeof(close_resp), &status);
    if(close_resp.ret!=0) 
      ret = close_resp.errno;
    else
      ret = close_resp.ret;
  }
#endif
  else if ( is_pipeid(fd) ) 
    ret = pipe_close(fd);

  return ret;
}
