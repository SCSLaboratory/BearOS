#pragma once

/* 
 * bear_compat_nfs.h: This file is used for compiling NFS and used
 * when -DBEARNFS is used.
 */

/* Switch to remove functionality not available in bear:
 * new ifdefs created only where necessary -- tries to use
 * standard switches provided below where possible
 */
#define REMOVE_UNSUPPORTED	

/* Set NFS standard configuration settings */
#undef HAVE_CONFIG_H
#undef AROS
#undef WIN32
#define HAVE_ARPA_INET_H 1
#undef NET_IF_H
#undef ANDROID
#undef HAVE_SYS_VFS_H
#undef HAVE_SYS_STATVFS_H
#undef HAVE_SYS_IOCTL_H
#define HAVE_SYS_SOCKET_H 1
#define HAVE_NETDB_H 1
#undef HAVE_NETINET_IN_H
#undef HAVE_NETINET_TCP_H
#undef HAVE_SYS_FILIO_H
#undef HAVE_SYS_SOCKIO_H
#define HAVE_POLL_H 1
#define HAVE_UNISTD_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UTIME_H 1

/* replacements for standard files */
#include <sys/time.h>	       
#include <sys/socket.h>	
#include <arpa/inet.h>

/* some missing defines */
#define _U_ 	
#define NI_NUMERICHOST	2
#define AF_INET6 10
#define IPPORT_RESERVED 1024

/* provides statvfs */
#ifndef STATVFS_DEFINED
struct statvfs {
        unsigned long  f_bsize;    /* filesystem block size */
        unsigned long  f_frsize;   /* fragment size */
        unsigned long  f_blocks;   /* size of fs in f_frsize units */
        unsigned long  f_bfree;    /* # free blocks */
        unsigned long  f_bavail;   /* # free blocks for unprivileged users */
        unsigned long  f_files;    /* # inodes */
        unsigned long  f_ffree;    /* # free inodes */
        unsigned long  f_favail;   /* # free inodes for unprivileged users */
        unsigned long  f_fsid;     /* filesystem ID */
        unsigned long  f_flag;     /* mount flags */
        unsigned long  f_namemax;  /* maximum filename length */
};
#define STATVFS_DEFINED
#endif

#ifndef FILE_CONTEXT_DEFINED
struct file_context {
        int is_nfs;
        int fd;
        struct nfs_context *nfs;
        struct nfsfh *nfsfh;
	struct nfsdir *nfsdir;
	struct nfsdirent *nfsdirent;
};
#define FILE_CONTEXT_DEFINED
#endif
