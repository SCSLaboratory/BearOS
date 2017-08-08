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
 * nfsd.c -- The nfsd implementation
 *
 * The nfs daemon keeps:
 *  -- a table of open files  -- created by nfs_open
 *  -- a table of open directories -- created by nfs_opendir
 */
#define _FILE_OFFSET_BITS 64	/* (ST) apparently this is important.. why here??? */

#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <syscall.h>		/* system calls */
#include <msg.h>		/* msgsend/msgrecv */

#include <sys/stat.h>
#include <sys/types.h>

#include <utils/bool.h>
#include <utils/shash.h>		/* sequenced hash table */

#include <sbin/nfsd.h>		/* daemon interface */

#include <libnfs.h>		/* the nfs interface */

//#define NFSD_DEBUG 1		/* if set: print all messages send & recvd */
//#define NFSD_SHOW 1		/* if set: show error messages */

/* defaults */
#define SERVERIP "192.168.0.2"
#define BEARROOT "/bear"	/* mount at /bear */
#define NFSD_BUFSIZE (3*1024*1024+337) /* nfs magic - who knows ??!!! (ST) */
#define MAXOPEN 50		       

typedef struct {	/* client structure signifying server and mount path */
  char *server;
  char *export;
  uint32_t mount_port;
  int is_finished;
}  client_t;

/*
 * The following structure MUST contain a key as a first entry in order
 * for the entry agnostic "lookup()" to operate correctly
 */
typedef struct {		/* file handle */
  int key;
  struct nfsfh *nfsfhp;
} fh_t;

typedef struct {		/* directory handle */
  int key;
  struct nfsdir *nfsdirp;
} dir_t;

/* An nfsd message buffer -- always aligned on a 64 bit boundary */
static nfsd_msg_t msgbuff;

/* helper functions */
static int nfsd_checkurl(char *url, char **serverp, char **pathp);
static int nfsd_init_daemon(int argc, char *argv[]);
static int nfsd_mount(void);
static int nfsd_access_root();
static void nfsd_close_file(void *fep);
static void nfsd_close_dir(void *direp);

/* hash table entry manipulations */
static fh_t *make_fh(int key,struct nfsfh *fp);
static dir_t *make_dir(int key,struct nfsdir *dp);
static int lookup(void* ep, const void *skey);

/* 
 * hidden internal variables used by the daemon
 */

/* the NFS context maintained by the daemon */
static struct nfs_context *nfs = NULL; 
/* the nfs client instance */
static client_t client;

/* a hashtable of open files */
static shashtable_t *openfiles;
/* a hashtable of open directories */
static shashtable_t *opendirs;


/* the next file handle that will be allocated */
static int nextfh;
/* the next directory handle that will be allocated */
static int nextdirh;

/* nfs file handle */
static struct nfsfh *nfsfhp;
/* nfs directory handle */
static struct nfsdir *nfsdirp;


/* 
 * main -- The main loop of the daemon
 *
 * Every message recieves a reply - typically DACK or DNAK
 */
int main(int argc, char *argv[]) {
  Msg_status_t status;		/* status of a recv */
  int done,ready,respsize,respval,dfh,dirh,flags,bytes,totalbytes,whence,cmd,val;
  uint64_t len,offset,current_offset;
  const nfsd_msg_t *msgp,*replyp; /* these values are NEVER modified */
  char *p,*path1p,*path2p,*dirnmp;
  int mypid;
  fh_t *fep;
  dir_t *direp;
  struct nfsdirent *nfsdirentp;
  struct stat st,*statp;
  char path[MAXPATHLEN];
  
  msgp = replyp = &msgbuff;	/* buffer reused for both send & recv */

  mypid = getpid();
  if(mypid!=NFSD) {
#ifdef NFSD_SHOW
    fprintf(stderr,"[nfsd: Initialized with pid %d, expected %d
",mypid,NFSD);
#endif
    exit(EXIT_FAILURE);
  }

  if(!nfsd_init_daemon(argc,argv)) {	/* create the file system context */
#ifdef NFSD_SHOW
    fprintf(stderr,"[NFSD: init failed, server: %s, mountpoint=%s
",
	    client.server,client.export);
#endif
    exit(EXIT_FAILURE);
  }

  /* 
   * while not done { receive msg, service it, respond to sender } 
   */
  for(done=FALSE, ready=FALSE; !done ; ) {
    /* block and wait to recieve a message */
    msgrecv(ANY,(void*)msgp, sizeof(msgbuff), &status); /* recieve */
#if NFSD_DEBUG
    nfsd_print_req(stdout,"nfsd-recv",msgp);
#endif 
    /* default reply TYPE is same as the recieved message */
    respval=DNAK;		/* default reply is NAK */
    respsize=sizeof(generic_dresp_t); /* default size is generic response */
    /* 
     * service the message THEN set up the reply (careful-reusing buffer) 
     */
    switch(msgtype(msgp)) {	/* what message type arrived */
    case DINIT:			/* try to mount the file system */
      if(ready=nfsd_mount()) 	/* if it mounts, good to go*/
	respval=DACK;	
      respvalue(replyp)=respval;
      break;
    case DPING:			/* ACK if able to  */
      if(ready && nfsd_access_root())
	respval=DACK;	
      respvalue(replyp)=respval;
      break;
    case DQUIT:			/* quit for testing only */
      if(ready && nfs!=NULL)	{ /* only quit if good & ready */
	shapply(openfiles,nfsd_close_file); /* close open files */
	shapply(opendirs,nfsd_close_dir);   /* close open dirs */
	nfs_destroy_context(nfs);      /* close nfs */
	respval = DACK;
	done=TRUE;		
      }
      respvalue(replyp)=respval;
      break;
    case NFSD_STAT:
      strncpy(path,((nfsd_pathop_req_t*)msgp)->paths,MAXPATHLEN);
      statp = &(((nfsd_stat_resp_t*)replyp)->st);
      respvalue(replyp)=nfs_stat(nfs,path,statp);
      respsize=sizeof(nfsd_stat_resp_t);
      break;
    case NFSD_OPEN:
      if(nextfh<FILEIDMAX) {
	/* build nfs file handle, put it in the hash table with key=dfh */
	flags = ((nfsd_openop_req_t*)msgp)->mode;
	/* create it if not reading and doesnt exist */
	if((flags!=O_RDONLY) && (nfs_stat(nfs,((nfsd_openop_req_t*)msgp)->path,&st)<0))
	  nfs_creat(nfs,((nfsd_openop_req_t*)msgp)->path,0660,&nfsfhp);
	if((respval=nfs_open(nfs,((nfsd_openop_req_t*)msgp)->path,flags,&nfsfhp))==0) {
	  if((fep=make_fh(nextfh,nfsfhp))!=NULL) { /* create hash entry */
	    shput(openfiles,(void*)fep,nextfh);  /* put in hash table */
	    respval=nextfh++; /* respond with the key */
	  }
	}	
      }
      respvalue(replyp)=respval; /* respond with error code or key */
      break;
    case NFSD_CLOSE:
      /* lookup the daemon file handle in the hash table */
      dfh=((nfsd_dhop_req_t*)msgp)->dh;
      if((fep=shremove(openfiles,lookup,dfh))!=NULL) {
	if((respval=nfs_close(nfs,((fh_t*)fep)->nfsfhp))==0) { /* close the file */
	  respval=DACK;
	  if(dfh==(nextfh-1))
	    nextfh--;
	}
	free(fep);
      }
      respvalue(replyp)=respval; /* ACK, error code, or NAK */
      break;
    case NFSD_READ:
      /* lookup the daemon file handle in the hash table */
      dfh=((nfsd_read_req_t*)msgp)->dfh; /* file handle */
      len=((nfsd_read_req_t*)msgp)->len; /* length to try to read */
      bytes=0;
      /* read the file */
      if((len<=MAXBUFF) && ((fep=shsearch(openfiles,lookup,dfh))!=NULL)) {
				p = (char*)(((nfsd_read_resp_t*)replyp)->rdbuff); /* p points to rdbuff */
				if((bytes=nfs_read(nfs,((fh_t*)fep)->nfsfhp,len,p))<=0)
	 			bytes=0;
      }
      /* response type is unchanged */
      ((nfsd_read_resp_t*)replyp)->len = bytes;
      respsize=sizeof(int)+sizeof(int)+bytes; /* type,len,bytes */
      break;
    case NFSD_CREAT:
      if(nextfh<FILEIDMAX) {
	/* create a file building the nfs file handle, then put it in the
	 * hash table with key=nextfh
	 */
	flags = ((nfsd_openop_req_t*)msgp)->mode;
	if((respval=nfs_creat(nfs,((nfsd_openop_req_t*)msgp)->path,flags,&nfsfhp))==0) {
	  if((fep=make_fh(nextfh,nfsfhp))!=NULL) { /* create hash entry */
	    shput(openfiles,(void*)fep,nextfh);	/* put in hash table */
	    respval=nextfh++;	/* respond with the key */
	  }
	}	
      }
      respvalue(replyp)=respval; /* respond with error code failure */
      break;
    case NFSD_WRITE:		/* fwrite(void *buff,uint64_t size,int fh);
      /* lookup the daemon file handle in the hash table */
      dfh=((nfsd_write_req_t*)msgp)->dfh;
      len=((nfsd_write_req_t*)msgp)->len;
      if((len<=MAXBUFF) && ((fep=shsearch(openfiles,lookup,dfh))!=NULL)) {
	p=(char*)(((nfsd_write_req_t*)msgp)->wrbuff);
	if((bytes=nfs_write(nfs,((fh_t*)fep)->nfsfhp,len,p))==len)
	  respvalue(replyp)=bytes; 
	/* otherwise respond with NAK */
      }
      break;
    case NFSD_SEEK:
      dfh=((nfsd_seek_req_t*)msgp)->dfh;
      offset=((nfsd_seek_req_t*)msgp)->offset;
      whence=((nfsd_seek_req_t*)msgp)->whence;
      if((fep=shsearch(openfiles,lookup,dfh))!=NULL) {
	if((respval=nfs_lseek(nfs,((fh_t*)fep)->nfsfhp,offset,whence,&current_offset))>=0)
	  respvalue(replyp)=(int)current_offset;
      }
      break;
    case NFSD_MKDIR:
      respvalue(replyp)=nfs_mkdir(nfs,((nfsd_pathop_req_t*)msgp)->paths);
      break;
    case NFSD_RMDIR:
      respvalue(replyp)=nfs_rmdir(nfs,((nfsd_pathop_req_t*)msgp)->paths);
      break;
    case NFSD_OPENDIR:
      /* build nfs dir handle, put in hash table with key=nextdirh */
      if((respval=nfs_opendir(nfs,((nfsd_pathop_req_t*)msgp)->paths,&nfsdirp))==0) {
	if((direp=make_dir(nextdirh,nfsdirp))!=NULL) { /* create hash entry */
	  shput(opendirs,(void*)direp,nextdirh);  /* put in hash table */
	  respval=nextdirh++; /* respond with the key */
	}
      }	
      respvalue(replyp)=respval; /* respond with error code or key */
      break;
    case NFSD_READDIR:
      /* lookup the daemon directory handle in the directory hash table */
      dirh=((nfsd_dhop_req_t*)msgp)->dh;
      dirnmp=((nfsd_readdir_resp_t*)replyp)->dirnm;
      if((direp=shsearch(opendirs,lookup,dirh))!=NULL) {
				nfsdirentp = nfs_readdir(nfs,((dir_t*)direp)->nfsdirp); /* read dir */
				if(nfsdirentp!=NULL) {
					respval=DACK;		/* got one -- respond ACK */
					strcpy(dirnmp,nfsdirentp->name); /* copy name */
				}
				else
					strcpy(dirnmp,""); /* empty string */
			}
			respvalue(replyp)=respval; /* ACK, error code, or NAK */
			respsize=(strlen(dirnmp)+1)+(3*sizeof(int)); /* str + 2 ints */
			break;
		case NFSD_CLOSEDIR:
			/* lookup the daemon directory handle in the directory hash table */
			dirh=((nfsd_dhop_req_t*)msgp)->dh;
			if((direp=shremove(opendirs,lookup,dirh))!=NULL) {
				nfs_closedir(nfs,((dir_t*)direp)->nfsdirp); /* close the file */
				respval=DACK;
				if(dirh==nextdirh-1) /* closing the last directory opened? */
					nextdirh--;	 /* reuse the dirhandle */
				free(direp);
			}
      respvalue(replyp)=respval; /* ACK, error code, or NAK */
      break;
    case NFSD_RENAME:
      path1p=((nfsd_pathop_req_t*)msgp)->paths;
      len=strlen(path1p)+1;
      path2p=path1p+len;
      respvalue(replyp)=nfs_rename(nfs,path1p,path2p);
      break;
    case NFSD_LINK:
      path1p=((nfsd_pathop_req_t*)msgp)->paths;
      len=strlen(path1p)+1;
      path2p=path1p+len;
      respvalue(replyp)=nfs_link(nfs,path1p,path2p);
      break;
    case NFSD_UNLINK:
      respvalue(replyp)=nfs_unlink(nfs,((nfsd_pathop_req_t*)msgp)->paths);
      break;
    case NFSD_FCNTL:
      /* lookup the daemon file handle in the hash table */
      dfh=((nfsd_fcntl_req_t*)msgp)->dfh; /* file handle */
      cmd=((nfsd_fcntl_req_t*)msgp)->cmd; /* cmd */
      val=((nfsd_fcntl_req_t*)msgp)->val; /* val */
      if((fep=shsearch(openfiles,lookup,dfh))!=NULL) {
	//	((fh_t*)fep)->nfsfhp
	switch(cmd) {
	case F_GETFD:
	case F_GETFL:
	  respvalue(replyp)=DACK;
	  break;
	case F_SETFD:
	case F_SETFL:
	  respvalue(replyp)=DACK;
	  break;
	default:
#ifdef NFSD_SHOW
	  fprintf(stderr,"fcntl: cmd=%d not implmented
",cmd);
#endif
	  break;
	}
      }
      break;
    default:			/* error! */
#ifdef NFSD_SHOW
      fprintf(stderr,"[error on recv: %d]
",msgtype(msgp));
#endif
      respvalue(replyp)=DNAK;	/* respond with NAK */
      break;
    }
    /* reply to sender using message specific size */
    msgsend(status.src,(void*)replyp, respsize); 
#if NFSD_DEBUG
    nfsd_print_resp(stdout,"nfsd-send",replyp);
#endif 
  }
  exit(EXIT_SUCCESS);
}

static void nfsd_close_file(void *fep) {
  nfs_close(nfs,((fh_t*)fep)->nfsfhp);
}

static void nfsd_close_dir(void *direp) {
  nfs_closedir(nfs,((dir_t*)direp)->nfsdirp);
}

/* 
 * nfsd_init_daemon -- returns TRUE if able to init nfs, otherwise
 * FALSE If successful, sets the server and path.
 */
static int nfsd_init_daemon(int argc, char *argv[]) {
  char *server = NULL, *path = NULL, *strp;

  if(argc!=2 || ! nfsd_checkurl(argv[1],&server,&path)) { /* valid url? */
    /* create default nfs mount point instead of using cmd line */
    server = strdup(SERVERIP);
    path = strdup(BEARROOT);	
  }
  /* here with valid server and path */
  client.server = server;
  client.export = path;
  client.is_finished = 0;
  /* build and save the context */
  nfs = nfs_init_context();
  if (nfs == NULL) {		/* set the daemon context */
    free(server);
    free(path);
    client.server = NULL;
    client.export = NULL;
    return FALSE;
  }
  nextfh = FILEIDMIN;
  openfiles = shopen(MAXOPEN); /* file handles are allocated in order */
  nextdirh = 3;		       /* first directory handle is 1 */
  opendirs = shopen(MAXOPEN);  /* directory handles allocated in order */
  return TRUE;
}

/*
 * nfsd_mount -- try to mount up to 5 times and then give up
 */
static int nfsd_mount(void) {
  int i,mountval;
  
  for(mountval=1,i=0; i<5 && mountval!=0; i++) {
    /* mount the file system */
    if((mountval=nfs_mount(nfs, client.server, client.export))!= 0) {
#ifdef NFSD_SHOW
      fprintf(stderr,"[Failed to mount nfs share : %s
]", nfs_get_error(nfs));
#endif
    }
  }
  if(mountval!=0) 
    return FALSE;
  return TRUE;
}

static int nfsd_access_root(void) {
  struct stat st;

  /* open the root of the file system */
  if(nfs_stat(nfs, "/",&st)<0) {
#ifdef NFSD_SHOW
    fprintf(stderr,"[Unable to access root directory (\"/\") %s", nfs_get_error(nfs));
#endif
    return FALSE;
  }
  return TRUE;
}

/*
 * check_url -- checks for a valid nfs url (nfs://<ipaddress><rootpath>)
 * sets the server and path if valid
 */
static int nfsd_checkurl(char *url, char **serverp, char **pathp) {
  char *server = NULL, *path = NULL, *strp;
  if (url == NULL) 		/* does url exit */
    return FALSE;
  if (strncmp(url, "nfs://", 6)) /* correct prefix? */
    return FALSE;
  server = strdup(url + 6);	/* duplicate the server string */
  if (server == NULL)		/* grab server string */
    return FALSE;
  if (server[0]=='/' || server[0]==' ') { /* invalid or empty?  */
    free(server);			   /* throw away duplicate */
    return FALSE;
  }
  strp = strchr(server, '/');	/* find start of path at end of server string */
  if (strp == NULL) {		/* no path! */
    free(server);		
    return FALSE;
  }
  path = strdup(strp);		/* duplicate the path string */
  if (path == NULL) {		/* empty  */
    free(server);
    free(path);
    return FALSE;
  }
  if (path[0] != '/') {		/* not an absolute path */
    free(server);
    free(path);
    return FALSE;
  }
  *serverp = server;		/* return the server and path */
  *pathp = path;
  return TRUE;
}


static fh_t *make_fh(int key,struct nfsfh *fp) {
  fh_t *fep;

  fep=NULL;
  if(fep=(fh_t*)malloc(sizeof(fh_t))) {
    fep->key = key;
    fep->nfsfhp = fp;
  }
  return fep;
}

static dir_t *make_dir(int key,struct nfsdir *dp) {
  dir_t *direp;

  direp=NULL;
  if(direp=(dir_t*)malloc(sizeof(dir_t))) {
    direp->key = key;
    direp->nfsdirp = dp;
  }
  return direp;
}

static int lookup(void* ep, const void *skeyp) {

  if( (*((int*)ep)) == (*((int*)skeyp)) )
    return(TRUE);
  else
    return(FALSE);
}
