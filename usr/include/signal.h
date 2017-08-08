/* 
 * signal.h header file copied to bear from MINIX by Scott Brookes
 */

/* The <signal.h> header defines all the ANSI and POSIX signals.
 * MINIX supports all the signals required by POSIX. They are defined below.
 * Some additional signals are also supported.
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stdint.h>

typedef int pid_t;

/* this was in type.h in MINIX, putting it here. */
typedef void (*sighandler_t) (int);
typedef void (*__sighandler_t) (int);

#if 0
#ifndef _SIGSET_T
#define _SIGSET_T
typedef unsigned long sigset_t;
#endif
#endif

/* Other types required by POSIX */
typedef int sig_atomic_t;

#define _NSIG             28	/* number of signals used */

#define SIGHUP             1	/* hangup */
#define SIGINT             2	/* interrupt (DEL) */
#define SIGQUIT            3	/* quit (ASCII FS) */
#define SIGILL             4	/* illegal instruction */
#define SIGTRAP            5	/* trace trap (not reset when caught) */
#define SIGABRT            6	/* IOT instruction */
#define SIGIOT             6	/* SIGABRT for people who speak PDP-11 */
#define SIGUNUSED          7	/* spare code */
#define SIGFPE             8	/* floating point exception */
#define SIGKILL            9	/* kill (cannot be caught or ignored) */
#define SIGUSR1           10	/* user defined signal # 1 */
#define SIGSEGV           11	/* segmentation violation */
#define SIGUSR2           12	/* user defined signal # 2 */
#define SIGPIPE           13	/* write on a pipe with no one to read it */
#define SIGALRM           14	/* alarm clock */
#define SIGTERM           15	/* software termination signal from kill */

#define SIGEMT             7	/* obsolete */
#define SIGBUS            10	/* obsolete */

#define SIGCHLD           17	/* child process terminated or stopped */
#define SIGCONT           18	/* continue if stopped */
#define SIGSTOP           19	/* stop signal */
#define SIGTSTP           20	/* interactive stop signal */
#define SIGTTIN           21	/* background process wants to read */
#define SIGTTOU           22	/* background process wants to write */
#define SIGPOLL           23    /* pollable event */
#define SIGPROF           24    /* profiling timer expired */
#define SIGVTALRM         25    /* virtual timer expired */
#define SIGURG            26    /* high bandwidth data is ready  at a socket */

#ifndef _SIGSET_T
#define _SIGSET_T
#if _NSIG <= 32
typedef uint32_t sigset_t;
#elif _NSIG <= 64
typedef uint64_t sigset_t;
#else 
typedef unsigned long sigset_t; /* hope that this is long enough */
#endif
#endif

/* ucontext_t type defined in sys/include/sigcontext.h */

/* special symbol for internal use             *  
 * lets us route sigprocmask through sigaction */
#define SIGPMASK          -100
#define SIGSUSP           -101
#define WAIT_ON_SIGNAL    -19

/* Macros used as function pointers. */
#define SIG_ERR    ((__sighandler_t) -1)        /* error return */
#define SIG_DFL	   ((__sighandler_t)  0)	/* default signal handling */
#define SIG_IGN	   ((__sighandler_t)  1)	/* ignore signal */
#define SIG_HOLD   ((__sighandler_t)  2)	/* block signal */
#define SIG_CATCH  ((__sighandler_t)  3)	/* catch signal */

/* for sigprocmask */
#define SIG_BLOCK                     4	 /* for blocking signals */
#define SIG_UNBLOCK                   5	 /* for unblocking signals */
#define SIG_SETMASK                   6	 /* for setting the signal mask */
#define SIG_PENDING ((__sighandler_t) 7) /* for the sig use only */

struct sigaction {
  __sighandler_t sa_handler;	/* SIG_DFL, SIG_IGN, or pointer to function */
  sigset_t sa_mask;		/* signals to be blocked during handler */
  int sa_flags;			/* special flags */
};

/* pulled from signal.h in minix3 source */
#define sigaddset(set, sig)      ((int) ((*set |=  (1 << (sig))) && 0))
#define sigdelset(set, sig)      ((int) ((*set &= ~(1 << (sig))) && 0))
#define sigemptyset(set)         ((int) ( *set  =   0))
#define sigfillset(set)          ((int) ((*set  =  ~0) && 0))
#define sigismember(set, sig)    ((*set & (1 << (sig))) ? 1 : 0)


/* signalling functions */
int kill(pid_t _pid, int _sig);
/* int killpg(pid_t _pid, int _sig); */

/* void psiginfo(const siginfo_t *, const char *); */
/* void psignal(int, const char *); */
/* int  pthread_kill(pthread_t, int); */
/* int  pthread_sigmask(int, const sigset_t *restrict, sigset_t *restrict); */

int raise (int _sig);
int sigaction(int _sig, const struct sigaction *_act, 
	      struct sigaction *_oact);
__sighandler_t signal(int _sig, __sighandler_t func);

/* int sigaltstack(const stack_t *restrict, stack_t *restrict); */

int sighold(int signo);
int sigignore(int signo);
/* int siginterrupt(int signo, int flag); */

/* int sigpause(int); */

int sigpending(sigset_t *_set); 
int sigprocmask(int _how,  const sigset_t *_set, sigset_t *_oset);
/* int sigqueue(pid_t pid, int signo, const union sigval); */

int sigrelse(int signo);
void (*sigset(int sig, void (*disp)(int)))(int);

int sigsuspend(const sigset_t *_sigmask);

#endif /* _SIGNAL_H */
