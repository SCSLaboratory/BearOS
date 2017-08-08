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
 * nfsd_if.c -- this file contains the implementation for functions
 * that communicate with nfsd. The prototypes are available through
 * the interface nfsd.h.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* getenv(), setenv() */
#include <stdint.h>		/* uint8_t */
#include <string.h>		/* strcmp(), strcpy() */
#include <msg.h>		/* Msg_status_t */

#include <sbin/daemon_msg_types.h>	/* generic message types */
#include <sbin/nfsd.h>		/* daemon interface */

//#define NFSD_IF_DEBUG 1		/* if set: shows every message */


static nfsd_msg_t msgbuff;
static const nfsd_msg_t *msgbuffp = &msgbuff;

/* helper functions */
static void extend_path(const char *path, char *dst);
static char *strip_path(char *path);
static void strip_chars(char *begin, char *end);

static int nfsd_openop(int dpid,int op,const char *path,int mode);
static int nfsd_dhop(int dpid,int op,int dh);
static int nfsd_pathop(int dpid,int op,const char *path1,const char *path2);
static int nfsd_send_recv_generic(int dpid,int op,int msgsize);


#ifdef NFSD_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes);
#endif
/*
 * Public Functions provided to communcate with the NFS daemon -- assume
 * bytes have been placed in the msgbuff
 */

/* 
 * Generic operations
 */
int nfsd_init(int dpid) {
  nfsd_send_recv_generic(dpid,DINIT,sizeof(generic_dinit_req_t));
}

int nfsd_ping(int dpid) {
  nfsd_send_recv_generic(dpid,DPING,sizeof(generic_dinit_req_t));
}

int nfsd_quit(int dpid) {
  nfsd_send_recv_generic(dpid,DQUIT,sizeof(generic_dinit_req_t));
}

/* 
 * Path operations 
 */
int nfsd_mkdir(int dpid,const char *path) {
  nfsd_pathop(dpid,NFSD_MKDIR,path,NULL);
}

int nfsd_rmdir(int dpid,const char *path) {
  nfsd_pathop(dpid,NFSD_RMDIR,path,NULL);
}

int nfsd_opendir(int dpid,const char *path) {
  nfsd_pathop(dpid,NFSD_OPENDIR,path,NULL);
}

int nfsd_unlink(int dpid,const char *path) {
  nfsd_pathop(dpid,NFSD_UNLINK,path,NULL);
}

int nfsd_rename(int dpid,const char *oldpath,const char *newpath) {
  nfsd_pathop(dpid,NFSD_RENAME,oldpath,newpath);
}

int nfsd_link(int dpid,const char *oldpath,const char *newpath) {
  nfsd_pathop(dpid,NFSD_LINK,oldpath,newpath);
}


/*
 * Open and Close operations  (note: opendir is a pathop)
 */
int nfsd_open(int dpid,const char *path,int mode) {
  nfsd_openop(dpid,NFSD_OPEN,path,mode);
}

int nfsd_creat(int dpid,const char *path,int mode) {
  nfsd_openop(dpid,NFSD_CREAT,path,mode);
}

int nfsd_close(int dpid,int dh) {
  nfsd_dhop(dpid,NFSD_CLOSE,dh);
}

int nfsd_closedir(int dpid,int dh) {
  nfsd_dhop(dpid,NFSD_CLOSEDIR,dh);
}

/* Other operations */

int nfsd_seek(int dpid,int dfh,uint64_t offset,int whence) {
  nfsd_seek_req_t *msgp;
  int msgsize;

  msgp = (nfsd_seek_req_t*)msgbuffp;
  msgp->dfh=dfh;		/* the daemons file handle */
  msgp->offset=offset;
  msgp->whence=whence;
  msgsize=sizeof(nfsd_seek_req_t);
  return nfsd_send_recv_generic(dpid,NFSD_SEEK,msgsize); /* ACK response expected */
}

int nfsd_readdir(int dpid,int dh,char *filenm) {
  nfsd_dhop_req_t *msgp;
  nfsd_readdir_resp_t *replyp;
  int msgsize;
  Msg_status_t status;

  /* send the daemon handle message */
  msgp = (nfsd_dhop_req_t*)msgbuffp;
  msgtype(msgp) = NFSD_READDIR;
  msgp->dh=dh;			/* the daemons handle */
  msgsize=sizeof(nfsd_dhop_req_t); 
  msgsend(dpid,(void*)msgbuffp,msgsize);
#ifdef NFSD_IF_DEBUG
  nfsd_print_req(stdout,"tnfsd-send",msgbuffp); /* print msg */
#endif
  
  /* wait for the response */
  replyp = (nfsd_readdir_resp_t*)msgbuffp; /* not a generic response */
  msgrecv(dpid,(void*)replyp,sizeof(nfsd_readdir_resp_t),&status);
#ifdef NFSD_IF_DEBUG
  nfsd_print_resp(stdout,"tnfsd-recv",msgbuffp); /* print reply */
#endif
  if(respvalue(replyp)==DACK)	/* got a directory name */
    strcpy(filenm,((nfsd_readdir_resp_t*)msgbuffp)->dirnm);
  return respvalue(replyp);
}


uint64_t nfsd_write(int dpid,char *datap,uint64_t wrbytes,int dfh) {
  nfsd_write_req_t *msgp;
  char *buffp;
  int i,ret,msgsize;
  uint64_t cnt;

  msgp = (nfsd_write_req_t*)msgbuffp;
  cnt=0;
  do {
    /* find the data entry in the message */
    buffp = nfsd_msg_wrbuff(msgp); 
    /* copy data to msg */
    for(i=0; (i<MAXBUFF && cnt<wrbytes);i++,cnt++)
      *(buffp+i) = *(datap+cnt);
    msgp->len=i;    
    msgp->dfh=dfh;		/* the daemons file handle */
    msgsize=i+(2*sizeof(int))+sizeof(uint64_t);
    if((ret=nfsd_send_recv_generic(dpid,NFSD_WRITE,msgsize))<0)
      return ret;
  } while(cnt<wrbytes);
  return cnt;
}


uint64_t nfsd_read(int dpid,char *datap,uint64_t rdbytes,int dfh) {
  nfsd_read_req_t *msgp;	/* the request message */
  nfsd_read_resp_t *replyp;	/* the response message */
  int msgsize,j;			/* size of the request message */
  Msg_status_t status;
  char *buffp,*p;
  uint64_t cnt,len,bytesread;

  cnt=0;
  len=rdbytes;
  p=datap;
  do {
    bytesread=0;
    if(rdbytes-cnt>MAXBUFF)
      len=MAXBUFF;
    else
      len=rdbytes-cnt;
    /* send the read message */
    msgp = (nfsd_read_req_t*)msgbuffp; /* msg formed in buffer */
    msgtype(msgp) = NFSD_READ;	     /* set the type */
    msgp->dfh = dfh;
    msgp->len = len;
    msgsize=sizeof(nfsd_read_req_t);
    msgsend(dpid,(void*)msgp,msgsize); /* send the message */
#ifdef NFSD_IF_DEBUG
    nfsd_print_req(stdout,"tnfsd-send",msgbuffp); 
#endif
    
    /* wait for the response */
    replyp = (nfsd_read_resp_t*)msgbuffp; /* not a generic response */
    msgrecv(dpid,(void*)replyp,sizeof(nfsd_read_resp_t),&status);
#ifdef NFSD_IF_DEBUG
    nfsd_print_resp(stdout,"tnfsd-recv",msgbuffp); /* print reply */
#endif
    if(replyp->len!=(status.bytes_rcvd-(2*sizeof(int)))) /* msglen-header */
      return cnt;				       /* error! */
    else {
      bytesread = (uint64_t)(replyp->len);
      /* copy out a buffer */
      buffp=nfsd_msg_rdbuff(replyp);
      for(j=0; j<bytesread; j++)
				*(p+j) = *(buffp+j);
      /* step on the output pointer */
      p += bytesread;
      /* one buffer less to read */
      cnt += bytesread;
    }
  } while(bytesread>0 && cnt<rdbytes);
  return cnt;
}


/*
 * nfsd_stat() -- generates a pathop message and a stat response
 */
int nfsd_stat(int dpid,const char *path,struct stat **statp) {
  nfsd_pathop_req_t *msgp;
  nfsd_stat_resp_t *replyp; 
  int msgsize;			
  int len;
  Msg_status_t status;

  /* send the stat message */
  msgp = (nfsd_pathop_req_t*)msgbuffp; /* msg formed in buffer */
  msgtype(msgp) = NFSD_STAT;
  strcpy(msgp->paths,path);	     /* copy path into message */
  len=strlen(path)+1;
  msgsize=len+sizeof(msgp->type); /* send only whats needed */
  msgsend(dpid,(void*)msgp,msgsize); /* send the message */
#ifdef NFSD_IF_DEBUG
  nfsd_print_req(stdout,"tnfsd-send",msgbuffp); 
#endif

  /* wait for the response */
  replyp = (nfsd_stat_resp_t*)msgbuffp; /* not a generic response */
  msgrecv(dpid,(void*)replyp,sizeof(nfsd_stat_resp_t),&status);
  *statp = &(((nfsd_stat_resp_t*)replyp)->st); /* return ptr to status */
#ifdef NFSD_IF_DEBUG
  nfsd_print_resp(stdout,"tnfsd-recv",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}

int nfsd_fcntl(int dpid,int dh,int cmd,int val) {
  nfsd_fcntl_req_t *msgp;
  int msgsize;

  /* send the close message */
  msgp = (nfsd_fcntl_req_t*)msgbuffp;
  msgp->dfh=dh;			/* the daemons file handle */
  msgp->cmd=cmd;
  msgp->val=val;
  msgsize=sizeof(nfsd_fcntl_req_t); 
  return nfsd_send_recv_generic(dpid,NFSD_FCNTL,msgsize);
}

/*
 * Form an absolute path from a file name
 */
char *abspath(char *file_name, char *resolved_name) {
  char buff[MAXPATHLEN];

  extend_path(file_name,buff);
  strcpy(resolved_name, strip_path(buff)); 
  return resolved_name;
}


/* PRIVATE functions (used to implement multiple interfaces) */

static int nfsd_dhop(int dpid,int op,int dh) {
  nfsd_dhop_req_t *msgp;
  int msgsize;

  /* send the close message */
  msgp = (nfsd_dhop_req_t*)msgbuffp;
  msgp->dh=dh;			/* the daemons file handle */
  msgsize=sizeof(nfsd_dhop_req_t); 
  return nfsd_send_recv_generic(dpid,op,msgsize);
}

/*
 * nfsd_openop - implements open and creat
 */
static int nfsd_openop(int dpid,int op,const char *path,int mode) {
  nfsd_openop_req_t *msgp;	/* the open message */
  int msgsize;			/* the size of the open message */

  /* send the open message */
  msgp = (nfsd_openop_req_t*)msgbuffp; /* msg formed in buffer */
  msgp->mode = mode;		     /* set the mode */
  strcpy(msgp->path,path);	     /* copy into message */
  /* send only whats needed */
  msgsize=(strlen(path)+1)+sizeof(msgp->type)+sizeof(msgp->mode); 
  return nfsd_send_recv_generic(dpid,op,msgsize);
}

/*
 * nfsd_pathop() -- implements all messages that operate on paths
 */
static int nfsd_pathop(int dpid,int op,const char *path1,const char *path2) {
  nfsd_pathop_req_t *msgp;	/* the directory message */
  int msgsize;			/* the size of the directory message */
  int len1,len2;

  /* send the stat message */
  msgp = (nfsd_pathop_req_t*)msgbuffp; /* msg formed in buffer */
  strcpy(msgp->paths,path1);	     /* copy path into message */
  len1=strlen(path1)+1;
  msgsize=len1+sizeof(msgp->type); /* send only whats needed */
  if(path2!=NULL) {		   /* add second path if needed */
    len2=strlen(path2)+1;
    strcpy((msgp->paths)+len1,path2);
    msgsize+=len2;
  }
  return nfsd_send_recv_generic(dpid,op,msgsize);
}


/*
 * nfsd_send_recv_generic -- assumes a message has been set up to send in
 * the msgbuff, add the operation type, sends it, then waits for a
 * generic response and returns the value associated with it.
 */
static int nfsd_send_recv_generic(int dpid,int op,int msgsize) {
  Msg_status_t status;
  generic_dresp_t *replyp;

  /* send to nfsd */
  msgtype(msgbuffp) = op;
  msgsend(dpid,(void*)msgbuffp,msgsize);
#ifdef NFSD_IF_DEBUG
  nfsd_print_req(stdout,"tnfsd-send",msgbuffp); /* print msg */
#endif

#ifdef NFSD_IF_DEBUG
  clear_buff((uint8_t*)msgbuffp,sizeof(generic_dresp_t));
#endif

  /* recv from nfsd */
  replyp = (generic_dresp_t*)msgbuffp;
  msgrecv(dpid,(void*)msgbuffp,sizeof(generic_dresp_t),&status);
#ifdef NFSD_IF_DEBUG
  nfsd_print_resp(stdout,"tnfsd-recv",msgbuffp); /* print reply */
#endif
  return respvalue(replyp);
}

/* PRIVATE functions (used to manipulate paths) */

static void extend_path(const char *path, char *dst) {
  if (path[0]=='~') {
    strcpy(dst, getenv("HOME"));
    strcat(dst, path+1);
  } 
  else if (path[0]=='/') {
    strcpy(dst, path);
    strcat(dst, "/");
  }
  else {
    strcpy(dst, getenv("PWD"));
    strcat(dst, "/");
    strcat(dst, path);
    strcat(dst, "/");
  }
}

static char *strip_path(char *path) {
  char *s, *priorSlash;
  while ((s=strstr(path, "/../"))!=NULL) {
    *s = 0;
    if ((priorSlash = strrchr(path, '/'))==NULL) { /* oops */ 
      *s = '/'; 
      break; 
    }
    strip_chars(priorSlash, s+3);
  }
  while ((s=strstr(path, "/./"))!=NULL) { 
    strip_chars(s, s+2); 
  }
  while ((s=strstr(path, "//"))!=NULL) { 
    strip_chars(s, s+1); 
  }
  s = path + (strlen(path)-1);
  if (s!=path && *s=='/') { 
    *s=0; 
  }
  return path;
}

static void strip_chars(char *begin, char *end) {
  while(*end!=0) { 
    *begin++ = *end++; 
  }
  *begin = 0;
}


#ifdef NFSD_IF_DEBUG
static clear_buff(uint8_t *p,uint64_t numbytes) {
  uint64_t i;
  for(i=0; i<numbytes; i++,p++) 
    *p = 0;
}
#endif 
