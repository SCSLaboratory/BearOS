/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIP_ARCH_H__
#define __LWIP_ARCH_H__

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#include "cc.h"

/** Temporary: define format string for size_t if not defined in cc.h */
#ifndef SZT_F
#define SZT_F U32_F
#endif /* SZT_F */
/** Temporary upgrade helper: define format string for u8_t as hex if not
    defined in cc.h */
#ifndef X8_F
#define X8_F  "02x"
#endif /* X8_F */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PACK_STRUCT_BEGIN
#define PACK_STRUCT_BEGIN
#endif /* PACK_STRUCT_BEGIN */

#ifndef PACK_STRUCT_END
#define PACK_STRUCT_END
#endif /* PACK_STRUCT_END */

#ifndef PACK_STRUCT_FIELD
#define PACK_STRUCT_FIELD(x) x
#endif /* PACK_STRUCT_FIELD */


#ifndef LWIP_UNUSED_ARG
#define LWIP_UNUSED_ARG(x) (void)x
#endif /* LWIP_UNUSED_ARG */ 


#ifdef LWIP_PROVIDE_ERRNO

#ifndef EPERM
#define  EPERM         1  /* Operation not permitted */
#endif
#ifndef ENOENT
#define  ENOENT        2  /* No such file or directory */
#endif
#ifndef ESRCH
#define  ESRCH         3  /* No such process */
#endif
#ifndef EINTR
#define  EINTR         4  /* Interrupted system call */
#endif
#ifndef EIO
#define  EIO           5  /* I/O error */
#endif
#ifndef ENXIO
#define  ENXIO         6  /* No such device or address */
#endif
#ifndef E2BIG
#define  E2BIG         7  /* Arg list too long */
#endif
#ifndef ENOEXEC
#define  ENOEXEC       8  /* Exec format error */
#endif
#ifndef EBADF
#define  EBADF         9  /* Bad file number */
#endif
#ifndef ECHILD
#define  ECHILD       10  /* No child processes */
#endif
#ifndef EAGAIN
#define  EAGAIN       11  /* Try again */
#endif
#ifndef ENOMEM
#define  ENOMEM       12  /* Out of memory */
#endif
#ifndef EACCES
#define  EACCES       13  /* Permission denied */
#endif
#ifndef EFAULT
#define  EFAULT       14  /* Bad address */
#endif
#ifndef ENOTBLK
#define  ENOTBLK      15  /* Block device required */
#endif
#ifndef EBUSY
#define  EBUSY        16  /* Device or resource busy */
#endif
#ifndef EEXIST
#define  EEXIST       17  /* File exists */
#endif
#ifndef EXDEV
#define  EXDEV        18  /* Cross-device link */
#endif
#ifndef ENODEV
#define  ENODEV       19  /* No such device */
#endif
#ifndef ENOTDIR
#define  ENOTDIR      20  /* Not a directory */
#endif
#ifndef EISDIR
#define  EISDIR       21  /* Is a directory */
#endif
#ifndef EINVAL
#define  EINVAL       22  /* Invalid argument */
#endif
#ifndef ENFILE
#define  ENFILE       23  /* File table overflow */
#endif
#ifndef EMFILE
#define  EMFILE       24  /* Too many open files */
#endif
#ifndef ENOTTY
#define  ENOTTY       25  /* Not a typewriter */
#endif
#ifndef ETXTBSY
#define  ETXTBSY      26  /* Text file busy */
#endif
#ifndef EFBIG
#define  EFBIG        27  /* File too large */
#endif
#ifndef ENOSPC
#define  ENOSPC       28  /* No space left on device */
#endif
#ifndef ESPIPE
#define  ESPIPE       29  /* Illegal seek */
#endif
#ifndef EROFS
#define  EROFS        30  /* Read-only file system */
#endif
#ifndef EMLINK
#define  EMLINK       31  /* Too many links */
#endif
#ifndef EPIPE
#define  EPIPE        32  /* Broken pipe */
#endif
#ifndef EDOM
#define  EDOM         33  /* Math argument out of domain of func */
#endif
#ifndef ERANGE
#define  ERANGE       34  /* Math result not representable */
#endif
#ifndef EDEADLK
#define  EDEADLK      35  /* Resource deadlock would occur */
#endif
#ifndef ENAMETOOLONG
#define  ENAMETOOLONG 36  /* File name too long */
#endif
#ifndef ENOLCK
#define  ENOLCK       37  /* No record locks available */
#endif
#ifndef ENOSYS
#define  ENOSYS       38  /* Function not implemented */
#endif
#ifndef ENOTEMPTY
#define  ENOTEMPTY    39  /* Directory not empty */
#endif
#ifndef ELOOP
#define  ELOOP        40  /* Too many symbolic links encountered */
#endif
#define  EWOULDBLOCK  EAGAIN  /* Operation would block */
#ifndef ENOMSG
#define  ENOMSG       42  /* No message of desired type */
#endif
#ifndef EIDRM
#define  EIDRM        43  /* Identifier removed */
#endif
#ifndef ECHRNG
#define  ECHRNG       44  /* Channel number out of range */
#endif
#ifndef EL2NSYNC
#define  EL2NSYNC     45  /* Level 2 not synchronized */
#endif
#ifndef EL3HLT
#define  EL3HLT       46  /* Level 3 halted */
#endif
#ifndef EL3RST
#define  EL3RST       47  /* Level 3 reset */
#endif
#ifndef ELNRNG
#define  ELNRNG       48  /* Link number out of range */
#endif
#ifndef EUNATCH
#define  EUNATCH      49  /* Protocol driver not attached */
#endif
#ifndef ENOCSI
#define  ENOCSI       50  /* No CSI structure available */
#endif
#ifndef EL2HLT
#define  EL2HLT       51  /* Level 2 halted */
#endif
#ifndef EBADE
#define  EBADE        52  /* Invalid exchange */
#endif
#ifndef EBADR
#define  EBADR        53  /* Invalid request descriptor */
#endif
#ifndef EXFULL
#define  EXFULL       54  /* Exchange full */
#endif
#ifndef ENOANO
#define  ENOANO       55  /* No anode */
#endif
#ifndef EBADRQC
#define  EBADRQC      56  /* Invalid request code */
#endif
#ifndef EBADSLT
#define  EBADSLT      57  /* Invalid slot */
#endif

#ifndef EDEADLOCK
#define  EDEADLOCK    EDEADLK
#endif

#ifndef EBFONT
#define  EBFONT       59  /* Bad font file format */
#endif
#ifndef ENOSTR
#define  ENOSTR       60  /* Device not a stream */
#endif
#ifndef ENODATA
#define  ENODATA      61  /* No data available */
#endif
#ifndef ETIME
#define  ETIME        62  /* Timer expired */
#endif
#ifndef ENOSR
#define  ENOSR        63  /* Out of streams resources */
#endif
#ifndef ENONET
#define  ENONET       64  /* Machine is not on the network */
#endif
#ifndef ENOPKG
#define  ENOPKG       65  /* Package not installed */
#endif
#ifndef EREMOTE
#define  EREMOTE      66  /* Object is remote */
#endif
#ifndef ENOLINK
#define  ENOLINK      67  /* Link has been severed */
#endif
#ifndef EADV
#define  EADV         68  /* Advertise error */
#endif
#ifndef ESRMNT
#define  ESRMNT       69  /* Srmount error */
#endif
#ifndef ECOMM
#define  ECOMM        70  /* Communication error on send */
#endif
#ifndef EPROTO
#define  EPROTO       71  /* Protocol error */
#endif
#ifndef EMULTIHOP
#define  EMULTIHOP    72  /* Multihop attempted */
#endif
#ifndef EDOTDOT
#define  EDOTDOT      73  /* RFS specific error */
#endif
#ifndef EBADMSG
#define  EBADMSG      74  /* Not a data message */
#endif
#ifndef EOVERFLOW
#define  EOVERFLOW    75  /* Value too large for defined data type */
#endif
#ifndef ENOTUNIQ
#define  ENOTUNIQ     76  /* Name not unique on network */
#endif
#ifndef EBADFD
#define  EBADFD       77  /* File descriptor in bad state */
#endif
#ifndef EREMCHG
#define  EREMCHG      78  /* Remote address changed */
#endif
#ifndef ELIBACC
#define  ELIBACC      79  /* Can not access a needed shared library */
#endif
#ifndef ELIBBAD
#define  ELIBBAD      80  /* Accessing a corrupted shared library */
#endif
#ifndef ELIBSCN
#define  ELIBSCN      81  /* .lib section in a.out corrupted */
#endif
#ifndef ELIBMAX
#define  ELIBMAX      82  /* Attempting to link in too many shared libraries */
#endif
#ifndef ELIBEXEC
#define  ELIBEXEC     83  /* Cannot exec a shared library directly */
#endif
#ifndef EILSEQ
#define  EILSEQ       84  /* Illegal byte sequence */
#endif
#ifndef ERESTART
#define  ERESTART     85  /* Interrupted system call should be restarted */
#endif
#ifndef ESTRPIPE
#define  ESTRPIPE     86  /* Streams pipe error */
#endif
#ifndef EUSERS
#define  EUSERS       87  /* Too many users */
#endif
#ifndef ENOTSOCK
#define  ENOTSOCK     88  /* Socket operation on non-socket */
#endif
#ifndef EDESTADDRREQ
#define  EDESTADDRREQ 89  /* Destination address required */
#endif
#ifndef EMSGSIZE
#define  EMSGSIZE     90  /* Message too long */
#endif
#ifndef EPROTOTYPE
#define  EPROTOTYPE   91  /* Protocol wrong type for socket */
#endif
#ifndef ENOPROTOOPT
#define  ENOPROTOOPT  92  /* Protocol not available */
#endif
#ifndef EPROTONOSUPPORT
#define  EPROTONOSUPPORT 93  /* Protocol not supported */
#endif
#ifndef ESOCKTNOSUPPORT
#define  ESOCKTNOSUPPORT 94  /* Socket type not supported */
#endif
#ifndef EOPNOTSUPP
#define  EOPNOTSUPP      95  /* Operation not supported on transport endpoint */
#endif
#ifndef EPFNOSUPPORT
#define  EPFNOSUPPORT    96  /* Protocol family not supported */
#endif
#ifndef EAFNOSUPPORT
#define  EAFNOSUPPORT    97  /* Address family not supported by protocol */
#endif
#ifndef EADDRINUSE
#define  EADDRINUSE      98  /* Address already in use */
#endif
#ifndef EADDRNOTAVAIL
#define  EADDRNOTAVAIL   99  /* Cannot assign requested address */
#endif
#ifndef ENETDOWN
#define  ENETDOWN       100  /* Network is down */
#endif
#ifndef ENETUNREACH
#define  ENETUNREACH    101  /* Network is unreachable */
#endif
#ifndef ENETRESET
#define  ENETRESET      102  /* Network dropped connection because of reset */
#endif
#ifndef ECONNABORTED
#define  ECONNABORTED   103  /* Software caused connection abort */
#endif
#ifndef ECONNRESET
#define  ECONNRESET     104  /* Connection reset by peer */
#endif
#ifndef ENOBUFS
#define  ENOBUFS        105  /* No buffer space available */
#endif
#ifndef EISCONN
#define  EISCONN        106  /* Transport endpoint is already connected */
#endif
#ifndef ENOTCONN
#define  ENOTCONN       107  /* Transport endpoint is not connected */
#endif
#ifndef ESHUTDOWN
#define  ESHUTDOWN      108  /* Cannot send after transport endpoint shutdown */
#endif
#ifndef ETOOMANYREFS
#define  ETOOMANYREFS   109  /* Too many references: cannot splice */
#endif
#ifndef ETIMEDOUT
#define  ETIMEDOUT      110  /* Connection timed out */
#endif
#ifndef ECONNREFUSED
#define  ECONNREFUSED   111  /* Connection refused */
#endif
#ifndef EHOSTDOWN
#define  EHOSTDOWN      112  /* Host is down */
#endif
#ifndef EHOSTUNREACH
#define  EHOSTUNREACH   113  /* No route to host */
#endif
#ifndef EALREADY
#define  EALREADY       114  /* Operation already in progress */
#endif
#ifndef EINPROGRESS
#define  EINPROGRESS    115  /* Operation now in progress */
#endif
#ifndef ESTALE
#define  ESTALE         116  /* Stale NFS file handle */
#endif
#ifndef EUCLEAN
#define  EUCLEAN        117  /* Structure needs cleaning */
#endif
#ifndef ENOTNAM
#define  ENOTNAM        118  /* Not a XENIX named type file */
#endif
#ifndef ENAVAIL
#define  ENAVAIL        119  /* No XENIX semaphores available */
#endif
#ifndef EISNAM
#define  EISNAM         120  /* Is a named type file */
#endif
#ifndef EREMOTEIO
#define  EREMOTEIO      121  /* Remote I/O error */
#endif
#ifndef EDQUOT
#define  EDQUOT         122  /* Quota exceeded */
#endif
#ifndef ENOMEDIUM
#define  ENOMEDIUM      123  /* No medium found */
#endif
#ifndef EMEDIUMTYPE
#define  EMEDIUMTYPE    124  /* Wrong medium type */
#endif

#ifndef ENSROK
#define ENSROK                    0 /* DNS server returned answer with no data */
#endif
#ifndef ENSRNODATA
#define ENSRNODATA              160 /* DNS server returned answer with no data */
#endif
#ifndef ENSRFORMERR
#define ENSRFORMERR             161 /* DNS server claims query was misformatted */
#endif
#ifndef ENSRSERVFAIL
#define ENSRSERVFAIL            162 /* DNS server returned general failure */
#endif
#ifndef ENSRNOTFOUND
#define ENSRNOTFOUND            163 /* Domain name not found */
#endif
#ifndef ENSRNOTIMP
#define ENSRNOTIMP              164 /* DNS server does not implement requested operation */
#endif
#ifndef ENSRREFUSED
#define ENSRREFUSED             165 /* DNS server refused query */
#endif
#ifndef ENSRBADQUERY
#define ENSRBADQUERY            166 /* Misformatted DNS query */
#endif
#ifndef ENSRBADNAME
#define ENSRBADNAME             167 /* Misformatted domain name */
#endif
#ifndef ENSRBADFAMILY
#define ENSRBADFAMILY           168 /* Unsupported address family */
#endif
#ifndef ENSRBADRESP
#define ENSRBADRESP             169 /* Misformatted DNS reply */
#endif
#ifndef ENSRCONNREFUSED
#define ENSRCONNREFUSED         170 /* Could not contact DNS servers */
#endif
#ifndef ENSRTIMEOUT
#define ENSRTIMEOUT             171 /* Timeout while contacting DNS servers */
#endif
#ifndef ENSROF
#define ENSROF                  172 /* End of file */
#endif
#ifndef ENSRFILE
#define ENSRFILE                173 /* Error reading file */
#endif
#ifndef ENSRNOMEM
#define ENSRNOMEM               174 /* Out of memory */
#endif
#ifndef ENSRDESTRUCTION
#define ENSRDESTRUCTION         175 /* Application terminated lookup */
#endif
#ifndef ENSRQUERYDOMAINTOOLONG
#define ENSRQUERYDOMAINTOOLONG  176 /* Domain name is too long */
#endif
#ifndef ENSRCNAMELOOP
#define ENSRCNAMELOOP           177 /* Domain name is too long */
#endif

#ifndef errno
extern int errno;
#endif

#endif /* LWIP_PROVIDE_ERRNO */

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_ARCH_H__ */
