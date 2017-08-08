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
 * nfsd_utils.c -- the nfsd printing utility.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>
#include <fcntl.h>		/* O_RDONLY,O_WRONLY etc */

#include <sbin/nfsd.h>		/* nfs daemon interface */

// #include <libnfs.h>		/* nfs interface */

/* helper functions */
static void print_pathop_req(FILE *logp,int numpaths,char *op,const nfsd_msg_t *msgp);
static void print_openop_req(FILE *logp,char *op,const nfsd_msg_t *msgp);

/* 
 * print_nfsd_msg -- a function that logs every type of message 
 * used by the daemon in a human readable format -- every message
 * used by the daemon must have an entry in this functions switch
 * statement. 
 *
 * NOTE: This function is exported for testing purposes and its name
 * is unique to the daemon.
 */
void nfsd_print_req(FILE *logp,char *op,const nfsd_msg_t *msgp) {
  int *p;
  int len;

  fprintf(logp,"%s: ",op);
  /* print the message in a readable format */
  switch(msgtype(msgp)) {
  case DINIT: fprintf(logp,"[init]
");	break;
  case DPING: fprintf(logp,"[ping]
");	break;
  case DQUIT: fprintf(logp,"[quit]
");	break;
  case NFSD_STAT: print_pathop_req(logp,1,"stat",msgp); break;
  case NFSD_OPEN: print_openop_req(logp,"open",msgp); break;
  case NFSD_CLOSE: fprintf(logp,"[close(%d)]
",((nfsd_dhop_req_t*)msgp)->dh); break;
  case NFSD_READ:		/* read */
    fprintf(logp,"[read(dfh=%d,len=%d)]
",
	    ((nfsd_read_req_t*)msgp)->dfh,
	    (int)((nfsd_read_req_t*)msgp)->len); /* len is uint64_t */
    break;
  case NFSD_CREAT: print_openop_req(logp,"creat",msgp); break;
  case NFSD_WRITE:		/* write */
    p=(int*)(((nfsd_write_req_t*)msgp)->wrbuff);
    len=(int)((nfsd_write_req_t*)msgp)->len;
    fprintf(logp,"[write(dfh=%d,len=%d,data=%d,%d,...%d)]
",
	    ((nfsd_write_req_t*)msgp)->dfh,len,*p,*(p+1),*(p+(len/sizeof(int))-1));
    break;
  case NFSD_SEEK:		/* seek */
    fprintf(logp,"[seek(dfh=%d,offset=%d,",
	    ((nfsd_seek_req_t*)msgp)->dfh,
	    ((int)((nfsd_seek_req_t*)msgp)->offset));
    switch(((nfsd_seek_req_t*)msgp)->whence) {
    case SEEK_SET: fprintf(logp,"SEEK_SET)]
"); break;
    case SEEK_CUR: fprintf(logp,"SEEK_CUR)]
"); break;
    case SEEK_END: fprintf(logp,"SEEK_END)]
"); break;
    default: fprintf(logp,"WHENCE?)]
"); break;
    }
    break;
  case NFSD_MKDIR: print_pathop_req(logp,1,"mkdir",msgp); break;
  case NFSD_RMDIR: print_pathop_req(logp,1,"rmdir",msgp); break;
  case NFSD_OPENDIR: print_pathop_req(logp,1,"opendir",msgp); break;
  case NFSD_READDIR: fprintf(logp,"[readdir(%d)]
",((nfsd_dhop_req_t*)msgp)->dh); break;
  case NFSD_CLOSEDIR: fprintf(logp,"[closedir(%d)]
",((nfsd_dhop_req_t*)msgp)->dh); break;
  case NFSD_RENAME: print_pathop_req(logp,2,"rename",msgp); break;
  case NFSD_LINK: print_pathop_req(logp,2,"link",msgp); break;
  case NFSD_UNLINK: print_pathop_req(logp,1,"unlink",msgp); break;
  case NFSD_FCNTL: 
    fprintf(logp,"[fcntl(dfh=%d,cmd=%d,val=%d)]
",
	    ((nfsd_fcntl_req_t*)msgp)->dfh,
	    ((nfsd_fcntl_req_t*)msgp)->cmd,
	    ((nfsd_fcntl_req_t*)msgp)->val); /* len is uint64_t */
    break;
  default:			/* unknown */
    fprintf(logp,"[unknown message: %d]
",msgtype(msgp)); 
    break;
  }
}


void nfsd_print_resp(FILE *logp,char *op,const nfsd_msg_t *msgp) {
  int *p;
  int len;

  fprintf(logp,"%s: ",op);
  /* print the message in a readable format */
  switch(msgtype(msgp)) {
  case DINIT: fprintf(logp,"[init"); break;
  case DPING: fprintf(logp,"[ping"); break;
  case DQUIT: fprintf(logp,"[quit"); break;
  case NFSD_STAT: fprintf(logp,"[stat"); break;
  case NFSD_OPEN: fprintf(logp,"[open"); break;
  case NFSD_CLOSE: fprintf(logp,"[close"); break;
  case NFSD_READ:				 
    p=(int*)(((nfsd_read_resp_t*)msgp)->rdbuff);
    len=((nfsd_read_resp_t*)msgp)->len;
    /* assumes data integers */
    fprintf(logp,"[read(len=%d,data=%d,%d,...%d)]
",
	    len,*p,*(p+1),*(p+(len/sizeof(int))-1));
    break;
  case NFSD_WRITE: fprintf(logp,"[write"); break;
  case NFSD_SEEK: fprintf(logp,"[seek"); break;	 
  case NFSD_MKDIR: fprintf(logp,"[mkdir"); break;
  case NFSD_RMDIR: fprintf(logp,"[rmdir"); break;
  case NFSD_OPENDIR: fprintf(logp,"[opendir"); break;
  case NFSD_READDIR: fprintf(logp,"[readdir(%s)",((nfsd_readdir_resp_t*)msgp)->dirnm); break;
  case NFSD_CLOSEDIR: fprintf(logp,"[closedir"); break;
  case NFSD_RENAME: fprintf(logp,"[rename"); break;
  case NFSD_LINK: fprintf(logp,"[link"); break;
  case NFSD_UNLINK: fprintf(logp,"[unlink"); break;
  case NFSD_FCNTL: fprintf(logp,"[fcntl"); break;
  default: fprintf(logp,"[unknown(%d)",msgtype(msgp)); break;
  }
  if(msgtype(msgp)!=NFSD_READ) {
    if(respvalue(msgp)==DACK)
      fprintf(logp,",ack]
");
    else if(respvalue(msgp)==DNAK)
      fprintf(logp,",nak]
");
    else 
      fprintf(logp,",%d]
",respvalue(msgp));
  }
}

static void print_pathop_req(FILE *logp,int numpaths,char *op,const nfsd_msg_t *msgp) {
  int len;
  char *path1p,*path2p;
  path1p=((nfsd_pathop_req_t*)msgp)->paths;
  fprintf(logp,"[%s(%s",op,path1p);
  if(numpaths==2) {
    len=strlen(path1p)+1; /* jump null */
    path2p=path1p+len;
    fprintf(logp,",%s",path2p);
  }
  fprintf(logp,")]
");
}

static void print_openop_req(FILE *logp,char *op,const nfsd_msg_t *msgp) {
  nfsd_openop_req_t *p = ((nfsd_openop_req_t*)msgp);

  fprintf(logp,"[%s(%s,",op,p->path);
  switch(p->mode) {
  case O_WRONLY: fprintf(logp,"\"w\")]
"); break;
  case O_RDONLY: fprintf(logp,"\"r\")]
"); break;
  case O_RDWR: fprintf(logp,"\"rw\")]
"); break;
  default: fprintf(logp,"MODE?)]
"); break;
  }
}
