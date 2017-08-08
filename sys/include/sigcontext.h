/* 
 * signal.h header file copied to bear from MINIX
 */

/* The <signal.h> header defines all the ANSI and POSIX signals.
 * MINIX supports all the signals required by POSIX. They are defined below.
 * Some additional signals are also supported.
 */

#ifndef _SIGCONTEXT_H
#define _SIGCONTEXT_H

#include <signal.h>
#include <mcontext.h>
#include <syscall.h>

/* kernel use */
#define SIG_IGNORED          1
#define SIG_BLOCKED          2
#define SIG_HANDLED          3
#define SIG_PROC_DESTROYED   4
#define SIG_PROC_BLOCKED     5
#define SIG_PROC_UNBLOCKED   6

struct sigcontext { 
  struct mcontext uc_mcontext;
  struct sigcontext *uc_link;
  sigset_t uc_sigmask;
};

typedef struct sigcontext ucontext_t; /* posix wants this */
typedef struct mcontext   mcontext_t; /* posix wants this */

#endif /* _SIGCONTEXT_H */
