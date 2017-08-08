#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

/* options */
#define WNOHANG    0x1
#define WUNTRACED  0x2
#define WCONTINUED 0x4

#ifndef ECHILD
/* error codes */
#define ECHILD -1
#endif

/* A status looks like:
      <2 bytes info> <2 bytes code>

      As defined by sys/include/proc.h
      <code> == 0x10, child has exited, info is the exit value
      <code> == 0x20, child has exited, info is the signal number.
      <code> == 0x02, child has stopped, info was the signal number.
      <code> == 0x04, child has continued, info is signal number.
*/   
#define WIFEXITED(w)	(((w) & 0x10) == 0x10)
#define WEXITSTATUS(w)	(((w) >> 8) & 0xff)

#define WIFSIGNALED(w)	(((w) & 0x20) == 0x20)
#define WTERMSIG(w)	(((w) >> 8) & 0xff)

#define WIFSTOPPED(w)	(((w) & 0x40))
#define WIFSTOPSIG(w)   (((w) >> 8) & 0xff)

#define WIFCONTINUED(w) (((w) & 0x04) == 0x04)

pid_t wait (int *);
pid_t waitpid (pid_t, int *, int);

#ifdef _COMPILING_NEWLIB
pid_t _wait (int *);
#endif

/* Provide prototypes for most of the _<systemcall> names that are
   provided in newlib for some compilers.  */
pid_t _wait (int *);

#ifdef __cplusplus
};
#endif

#endif
