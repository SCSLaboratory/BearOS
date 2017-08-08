/* errno is not a global variable, because that would make using it
   non-reentrant.  Instead, its address is returned by the function
   __errno.  */

#ifndef _SYS_ERRNO_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _SYS_ERRNO_H_

#include <sys/reent.h>

#ifndef _REENT_ONLY
#define errno (*__errno())
extern int *__errno _PARAMS ((void));
#endif

/* Please don't use these variables directly.
   Use strerror instead. */
extern __IMPORT _CONST char * _CONST _sys_errlist[];
extern __IMPORT int _sys_nerr;
#ifdef __CYGWIN__
extern __IMPORT const char * const sys_errlist[];
extern __IMPORT int sys_nerr;
extern __IMPORT char *program_invocation_name;
extern __IMPORT char *program_invocation_short_name;
#endif

#define __errno_r(ptr) ((ptr)->_errno)

#ifndef EPERM
#define	EPERM 1		/* Not super-user */
#endif
#ifndef ENOENT
#define	ENOENT 2	/* No such file or directory */
#endif
#ifndef ESRCH
#define	ESRCH 3		/* No such process */
#endif
#ifndef EINTR
#define	EINTR 4		/* Interrupted system call */
#endif
#ifndef EIO
#define	EIO 5		/* I/O error */
#endif
#ifndef ENXIO
#define	ENXIO 6		/* No such device or address */
#endif
#ifndef E2BIG
#define	E2BIG 7		/* Arg list too long */
#endif
#ifndef ENOEXEC
#define	ENOEXEC 8	/* Exec format error */
#endif
#ifndef EBADF
#define	EBADF 9		/* Bad file number */
#endif
#ifndef ECHILD
#define	ECHILD 10	/* No children */
#endif
#ifndef EAGAIN
#define	EAGAIN 11	/* No more processes */
#endif
#ifndef ENOMEM
#define	ENOMEM 12	/* Not enough core */
#endif
#ifndef EACCES
#define	EACCES 13	/* Permission denied */
#endif
#ifndef EFAULT
#define	EFAULT 14	/* Bad address */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define	ENOTBLK 15	/* Block device required */
#endif
#ifndef EBUSY
#define	EBUSY 16	/* Mount device busy */
#endif
#ifndef EEXIST
#define	EEXIST 17	/* File exists */
#endif
#ifndef EXDEV
#define	EXDEV 18	/* Cross-device link */
#endif
#ifndef ENODEV
#define	ENODEV 19	/* No such device */
#endif
#ifndef ENOTDIR
#define	ENOTDIR 20	/* Not a directory */
#endif
#ifndef EISDIR
#define	EISDIR 21	/* Is a directory */
#endif
#ifndef EINVAL
#define	EINVAL 22	/* Invalid argument */
#endif
#ifndef ENFILE
#define	ENFILE 23	/* Too many open files in system */
#endif
#ifndef EMFILE
#define	EMFILE 24	/* Too many open files */
#endif
#ifndef ENOTTY
#define	ENOTTY 25	/* Not a typewriter */
#endif
#ifndef ETXTBSY
#define	ETXTBSY 26	/* Text file busy */
#endif
#ifndef EFBIG
#define	EFBIG 27	/* File too large */
#endif
#ifndef ENOSPC
#define	ENOSPC 28	/* No space left on device */
#endif
#ifndef ESPIPE
#define	ESPIPE 29	/* Illegal seek */
#endif
#ifndef EROFS
#define	EROFS 30	/* Read only file system */
#endif
#ifndef EMLINK
#define	EMLINK 31	/* Too many links */
#endif
#ifndef EPIPE
#define	EPIPE 32	/* Broken pipe */
#endif
#ifndef EDOM
#define	EDOM 33		/* Math arg out of domain of func */
#endif
#ifndef ERANGE
#define	ERANGE 34	/* Math result not representable */
#endif
#ifndef ENOMSG
#define	ENOMSG 35	/* No message of desired type */
#endif
#ifndef EIDRM
#define	EIDRM 36	/* Identifier removed */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define	ECHRNG 37	/* Channel number out of range */
#define	EL2NSYNC 38	/* Level 2 not synchronized */
#define	EL3HLT 39	/* Level 3 halted */
#define	EL3RST 40	/* Level 3 reset */
#define	ELNRNG 41	/* Link number out of range */
#define	EUNATCH 42	/* Protocol driver not attached */
#define	ENOCSI 43	/* No CSI structure available */
#define	EL2HLT 44	/* Level 2 halted */
#endif
#ifndef EDEADLK
#define	EDEADLK 45	/* Deadlock condition */
#endif
#ifndef ENOLCK
#define	ENOLCK 46	/* No record locks available */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define EBADE 50	/* Invalid exchange */
#define EBADR 51	/* Invalid request descriptor */
#define EXFULL 52	/* Exchange full */
#define ENOANO 53	/* No anode */
#define EBADRQC 54	/* Invalid request code */
#define EBADSLT 55	/* Invalid slot */
#define EDEADLOCK 56	/* File locking deadlock error */
#define EBFONT 57	/* Bad font file fmt */
#endif
#ifndef ENOSTR
#define ENOSTR 60	/* Device not a stream */
#endif
#ifndef ENODATA
#define ENODATA 61	/* No data (for no delay io) */
#endif
#ifndef ETIME
#define ETIME 62	/* Timer expired */
#endif
#ifndef ENOSR
#define ENOSR 63	/* Out of streams resources */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ENONET 64	/* Machine is not on the network */
#define ENOPKG 65	/* Package not installed */
#define EREMOTE 66	/* The object is remote */
#endif
#ifndef ENOLINK
#define ENOLINK 67	/* The link has been severed */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define EADV 68		/* Advertise error */
#define ESRMNT 69	/* Srmount error */
#define	ECOMM 70	/* Communication error on send */
#endif
#ifndef EPROTO
#define EPROTO 71	/* Protocol error */
#endif
#ifndef EMULTIHOP
#define	EMULTIHOP 74	/* Multihop attempted */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define	ELBIN 75	/* Inode is remote (not really error) */
#define	EDOTDOT 76	/* Cross mount point (not really error) */
#endif
#ifndef EBADMSG
#define EBADMSG 77	/* Trying to read unreadable message */
#endif
#ifndef EFTYPE
#define EFTYPE 79	/* Inappropriate file type or format */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ENOTUNIQ 80	/* Given log. name not unique */
#define EBADFD 81	/* f.d. invalid for this operation */
#define EREMCHG 82	/* Remote address changed */
#define ELIBACC 83	/* Can't access a needed shared lib */
#define ELIBBAD 84	/* Accessing a corrupted shared lib */
#define ELIBSCN 85	/* .lib section in a.out corrupted */
#define ELIBMAX 86	/* Attempting to link in too many libs */
#define ELIBEXEC 87	/* Attempting to exec a shared library */
#endif
#ifndef ENOSYS
#define ENOSYS 88	/* Function not implemented */
#endif
#ifdef __CYGWIN__
#define ENMFILE 89      /* No more files */
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY 90	/* Directory not empty */
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 91	/* File or path name too long */
#endif
#ifndef ELOOP
#define ELOOP 92	/* Too many symbolic links */
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP 95	/* Operation not supported on transport endpoint */
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT 96 /* Protocol family not supported */
#endif
#ifndef ECONNRESET
#define ECONNRESET 104  /* Connection reset by peer */
#endif
#ifndef ENOBUFS
#define ENOBUFS 105	/* No buffer space available */
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 106 /* Address family not supported by protocol family */
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE 107	/* Protocol wrong type for socket */
#endif
#ifndef ENOTSOCK
#define ENOTSOCK 108	/* Socket operation on non-socket */
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT 109	/* Protocol not available */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ESHUTDOWN 110	/* Can't send after socket shutdown */
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED 111	/* Connection refused */
#endif
#ifndef EADDRINUSE
#define EADDRINUSE 112		/* Address already in use */
#endif
#ifndef ECONNABORTED
#define ECONNABORTED 113	/* Connection aborted */
#endif
#ifndef ENETUNREACH
#define ENETUNREACH 114		/* Network is unreachable */
#endif
#ifndef ENETDOWN
#define ENETDOWN 115		/* Network interface is not configured */
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 116		/* Connection timed out */
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN 117		/* Host is down */
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH 118	/* Host is unreachable */
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 119		/* Connection already in progress */
#endif
#ifndef EALREADY
#define EALREADY 120		/* Socket already connected */
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ 121	/* Destination address required */
#endif
#ifndef EMSGSIZE
#define EMSGSIZE 122		/* Message too long */
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 123	/* Unknown protocol */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ESOCKTNOSUPPORT 124	/* Socket type not supported */
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL 125	/* Address not available */
#endif
#ifndef ENETRESET
#define ENETRESET 126
#endif
#ifndef EISCONN
#define EISCONN 127		/* Socket is already connected */
#endif
#ifndef ENOTCONN
#define ENOTCONN 128		/* Socket is not connected */
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS 129
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define EPROCLIM 130
#define EUSERS 131
#endif
#ifndef EDQUOT
#define EDQUOT 132
#endif
#ifndef ESTALE
#define ESTALE 133
#endif
#ifndef ENOTSUP
#define ENOTSUP 134		/* Not supported */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ENOMEDIUM 135   /* No medium (in tape drive) */
#endif
#ifdef __CYGWIN__
#define ENOSHARE 136    /* No such host or network path */
#define ECASECLASH 137  /* Filename exists with different case */
#endif
#ifndef EILSEQ
#define EILSEQ 138
#endif
#ifndef EOVERFLOW
#define EOVERFLOW 139	/* Value too large for defined data type */
#endif
#ifndef ECANCELED
#define ECANCELED 140	/* Operation canceled */
#endif
#ifndef ENOTRECOVERABLE
#define ENOTRECOVERABLE 141	/* State not recoverable */
#endif
#ifndef EOWNERDEAD
#define EOWNERDEAD 142	/* Previous owner died */
#endif
#ifdef __LINUX_ERRNO_EXTENSIONS__
#define ESTRPIPE 143	/* Streams pipe error */
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN	/* Operation would block */
#endif

#ifndef ELASTERROR
#define __ELASTERROR 2000	/* Users can add values starting here */
#endif

#ifdef __cplusplus
}
#endif
#endif /* _SYS_ERRNO_H */
