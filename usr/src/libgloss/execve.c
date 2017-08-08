/*
 * Stub version of execve.
 */

#include <_ansi.h>
#include <string.h>
#include <msg.h>
#include <time.h>
#include <syscall.h>
#include <stdio.h>
#include <sys/stat.h>
#include <malloc.h>
#include <sys/fcntl.h>

#include <sbin/nfsd.h>

//#define EXECVE_DEBUG 1

#define streq(s1,s2) (strcmp(s1,s2)==0)

int
_DEFUN (execve, (name, argv, env),
        char  *name  _AND
        char **argv  _AND
        char **env)
{
  Exec_req_t req;
  Exec_resp_t resp;
  Msg_status_t status;
  int len, argc, bytes_read, count, num_env, dfh, size, i, res;
  char *buf;
  struct stat *statp;
  char abs_path[MAX_FNAME_SZ];

  memset(&req, 0, sizeof(Exec_req_t));
  req.type = SC_EXEC;
  req.argv = argv;		/* save argv and env in request */
  req.env = env; 

  /* Verify the argc/argv */
  if(argv==NULL) 
    return -1;
  argc = 0;
  while(*argv != NULL) {
#ifdef EXECVE_DEBUG
    printf("execve: argv=%s\n",*(argv));
#endif
    argv++;
    argc++;
  }
  req.argc = argc;		/* save argc in request */
#ifdef EXECVE_DEBUG
  printf("execve: argc=%d\n",argc);
#endif

  /* Verify env */
  num_env = 0;
  if(env!=NULL) {
    while(*env != NULL) {
#ifdef EXECVE_DEBUG
      printf("execve: env=%s\n",*(env));
#endif
      env++;
      num_env++;
    }
  }
  req.num_env = num_env;	/* save num_env in request */

  len = strlen(name)+1;
  memset(req.fname,0,MAX_FNAME_SZ);
  strncpy(req.fname,name,len);
  req.binary_location = NULL;
  req.binary_size = 0;


#if ( !defined(STANDALONE) )
  strncpy(abs_path,"/bin/",MAX_FNAME_SZ);
  strncat(abs_path,name,MAX_FNAME_SZ);
  abs_path[MAX_FNAME_SZ-1]='\0';
  if((nfsd_stat(NFSD,abs_path,&statp))<0)
    return -1;
  size = statp->st_size;
  req.binary_location = (char*)malloc(size);
  for(i=0; i<size; i+=8) 
    *(uint64_t*)(req.binary_location+i)=0;
  if((dfh=nfsd_open(NFSD,abs_path,O_RDONLY))<0) /* open the file */
    return -1;
  if(nfsd_seek(NFSD,dfh,0,SEEK_CUR)<0) /* go to front */
    return -1;
  /*TODO where does binary_location get free'ed?!?! */
  if((bytes_read=nfsd_read(NFSD,req.binary_location,size,dfh))<0) 
    return -1;
  req.binary_size = size;
  if(nfsd_close(NFSD,dfh)<0)	/* close the file handle */
    return -1;
#endif
  /* tell the kernel to replace this code */
  msgsend(SYS, &req, sizeof(Exec_req_t));
  /* return here only if kernel failed to do exec */
  msgrecv(SYS, &resp, sizeof(Exec_resp_t), &status);
  /* on error, return the result */
  return resp.ret;
}

