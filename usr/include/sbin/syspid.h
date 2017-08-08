#pragma once
/*
 * syspid.h -- PIDs for system processes
 */
#define PROC_NONE  (0)		/* no process may have this pid */
#define ANY       (-1)		/* allows msgrecv(...) from any process */
#define HARDWARE  (-2)		/* hardware interrupts */
#define IDLE_PROC (-3)		/* used when no processes are ready */
#define SYS       (-4)		/* destination for system calls */

/* userland daemons */
#define VGAD      (-5)		/* video daemon */
#define KBD       (-6)		/* keyboard daemon */
#define NETD      (-7)		/* network daemon (operations on sockets) */
#define E1000D    (-8)		/* E1000 network driver daemon */
#define NFSD      (-9)		/* NFS daemon (operations on files) */
#define STATD     (-10)		/* NFS keep alive daemon */
#define SYSD      (-11)		/* system daemon - operations on ids, top of process hierachy */
#define PIPED     (-12)		/* pipe daemon (operations on pipes) */
#define RSHD      (-13)		/* remote shell daemon */

/* The generic daemon -- used only for generating new daemons */
#define DAEMOND (-9999)		/* ID - only used when daemon installed */



