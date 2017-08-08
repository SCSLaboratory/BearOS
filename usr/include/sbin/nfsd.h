#pragma once
/*
 * nfsd.h -- The nfsd interface 
 *
 * NFSD Daemon Interface -- This interface definition is provided to
 * facilitate communcation with the nfsd daemon. Generally interface
 * functions are implemented in nfsd_if.c and typically return >=0 if
 * successful; otherwise a negative number signifying an error is
 * returned. The file usr/test/tnfsd.c shows how to use the interface.
 */
#include <_ansi.h>
#include <stdio.h>
#include <limits.h>
#include <syscall.h>
#include <msg.h>
#include <fcntl.h>	      /* O_RDONLY,O_WRONLY etc */
#include <sbin/daemon_msg_types.h> /* generic message formats used by all daemons */
#include <sbin/syspid.h>	      /* defines NFSD */

/* first third for files, second third for sockets, last third for pipes */
#define FILEIDMIN (3)
#define FILEIDMAX (INT_MAX/3)
#define is_fileid(x) (x>=FILEIDMIN && x<=FILEIDMAX)
#define is_stdioid(x) (x>=0 && x<=2)

//#include <sys/unistd.h>		/* MAXPATHLEN */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define MAXBUFF 1024	    /* 1MBytes is the largest file */

/* 
 * Daemon Message Types: 
 * daemon_msg_types.h provides:
 * DINIT=1
 * DPING=2
 * DQUIT=3 
 */
#define NFSD_STAT 4		
#define NFSD_OPEN 5
#define NFSD_CLOSE 6
#define NFSD_READ 7
#define NFSD_CREAT 8
#define NFSD_WRITE 9
#define NFSD_SEEK 10
#define NFSD_MKDIR 11
#define NFSD_RMDIR 12
#define NFSD_OPENDIR 13
#define NFSD_READDIR 14
#define NFSD_CLOSEDIR 15
#define NFSD_RENAME 16
#define NFSD_LINK 17
#define NFSD_UNLINK 18
#define NFSD_FCNTL 19

/*
 * Place daemon specific request & response message formats here. Note
 * that the generic response messages have the form:
 *
 * typedef struct {
 *   int type;		     // one of the above message types
 *   int value;		     // 0->successful; otherwise -errno
 * } generic_dresp_t;
 * 
 * NOTE: variable length fields are placed last in all structures
 * allowing only the component of them actually filled to be
 * transmitted
 */

/* Request Messages */
typedef struct {		
  int type; 			/* NFSD_OPEN, NFSD_CREAT */
  int mode;			/* normal flags */
  char path[MAXPATHLEN];	/* absolute path */
} nfsd_openop_req_t;		/* generic response, value>0 */

typedef struct {
  int type; 			/* NFSD_CLOSE, NFSD_CLOSEDIR, NFSD_READDIR */
  int dh;	     		/* daemon handle operation */
} nfsd_dhop_req_t;		

typedef struct {
  int type; 			/* NFSD_FCNTL */
  int dfh;	     		/* daemon handle operation */
  int cmd;			/* command: F_SETFL,GETFL,SETFD,GETFD */
  int val;			/* value if used */
} nfsd_fcntl_req_t;		

typedef struct {
  int type; 			/* NFSD_READ */
  int dfh;			/* daemon file handle */
  uint64_t len;			/* bytes to read */
} nfsd_read_req_t;

typedef struct {
  int type; 			/* NFSD_WRITE */
  int dfh;			/* daemon file handle */
  uint64_t len;			/* bytes to write */
  uint8_t wrbuff[MAXBUFF];	/* data to write */
} nfsd_write_req_t;		/* generic response */

typedef struct {
  int type; 			/* NFSD_SEEK */
  int dfh;			/* daemon file handle */
  uint64_t offset;		/* byte offset to seek */
  int whence;			/* SEEK_SET,SEEK_CUR,SEEK_END */
} nfsd_seek_req_t;		/* generic response */

typedef struct {		
  int type; 			/* NFSD_<PATHOP> - ALL ops using paths */
  char paths[2*MAXPATHLEN];	/* absolute paths */
} nfsd_pathop_req_t;		/* generic response, value>0 */

/* Response Messages */
typedef struct {
  int type; 			/* NFSD_READ response */
  int len;			/* number of bytes read */
  uint8_t rdbuff[MAXBUFF];	/* bytes read */
} nfsd_read_resp_t;		

typedef struct {		/* NFSD_STAT response */
  int type;
	unsigned int tag;
  int value;
  struct stat st;		/* result from stat operation */
} nfsd_stat_resp_t;

typedef struct {		
  int type; 			/* NFSD_READDIR */
	unsigned int tag;
  int value;
  char dirnm[MAXPATHLEN];		/* subdirectory name */
} nfsd_readdir_resp_t;		

/* macros for accessing buffers in read & write messages */
#define nfsd_msg_rdbuff(msgp)\
  ((void*)(((nfsd_read_resp_t*)msgp)->rdbuff))
#define nfsd_msg_wrbuff(msgp)\
  ((void*)(((nfsd_write_req_t*)msgp)->wrbuff))

/*
 * The Daemon Message Buffer -- always aligned on a 64 bit boundary
 */
typedef union { 
  generic_dresp_t dresp;	/* generic response */
  generic_dinit_req_t dinit;	/* init */
  generic_dping_req_t dping;	/* ping */
  generic_dquit_req_t dquit;	/* quit */
  nfsd_openop_req_t dopenop;	/* open,creat */
  nfsd_dhop_req_t ddhop;	/* close, closedir, readdir */
  nfsd_read_req_t dread;	/* read */
  nfsd_write_req_t dwrite;	/* write */
  nfsd_seek_req_t dseek;	/* seek */
  nfsd_pathop_req_t ddirop;	/* mkdir,rmdir,opendir,closedir,unlink */
  nfsd_read_resp_t dreadresp;	/* read response */
  nfsd_stat_resp_t dstatresp;	/* stat response */
  nfsd_readdir_resp_t dreaddirresp; /* readdir response */
  nfsd_fcntl_req_t dfcntl;	    /* fcntl */
} nfsd_msg_t __attribute__ ((aligned(sizeof(uint64_t))));

/*
 * Message packaging routines implemented in usr/sbin/nfsd/nfsd_if.c
 * Generally functions return 0 for success; otherwise
 * -(err). However, both open and opendir return a positive number
 * representing a daemon handle; read and write return a positive
 * number representing the number of bytes read or written.
 */
/* file operations */
int nfsd_init(int dpid);
int nfsd_ping(int dpid);
int nfsd_quit(int dpid);
int nfsd_open(int dpid,const char *path,int mode); 
int nfsd_creat(int dpid,const char *path,int mode);
int nfsd_stat(int dpid,const char *path,struct stat **statp);
int nfsd_seek(int dpid,int dfh,uint64_t offset,int whence);
uint64_t nfsd_read(int dpid,char *datap,uint64_t rdbytes,int dfh);
uint64_t nfsd_write(int dpid,char *datap,uint64_t wrbytes,int dfh);
int nfsd_close(int dpid,int dh);
int nfsd_fcntl(int dpid,int dfh, int cmd, int val);

/* directory operations */
int nfsd_mkdir(int dpid,const char *path);
int nfsd_rmdir(int dpid,const char *path);
int nfsd_opendir(int dpid,const char *path);
int nfsd_readdir(int dpid,int dh,char *filenm);
int nfsd_closedir(int dpid,int dh);

/* link operations */
int nfsd_unlink(int dpid,const char *path);
int nfsd_rename(int dpid,const char *oldpath,const char *newpath);
int nfsd_link(int dpid,const char *oldpath,const char *newpath);

/* utility function that forms an absolute path from a file name */
char *abspath(char *file_name, char *absolute_path);

/* Daemon printing functions provided by nfsd_utils.c */
void nfsd_print_msg(FILE *logp,char *op,const nfsd_msg_t *msgp);
void nfsd_print_response(FILE *logp,char *op,const nfsd_msg_t *msgp);

