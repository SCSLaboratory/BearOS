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
 * signal.c 
 * 
 * This file contains signal-related system calls. In order to decrease 
 *   the number of routes from user to kernel space, most of them route 
 *   through sigaction. These calls are all defined in the posix standard, 
 *   less _signal_return which is explained below.
 * 
 * Two signal-related functions are not here 
 * --> kill - signal a process - found in libgloss
 * --> raise - signal yourself (ie kill(getpid(), signo)) - found in newlib
 * 
 */

#include <signal.h>
#include <syscall.h>
#include <unistd.h>

int _signal_return(void);

/* signal */
__sighandler_t signal(int _sig, __sighandler_t func) {

  int ret_val;
  struct sigaction act, oldact;

  act.sa_handler = func;

  ret_val = sigaction(_sig, &act, &oldact);

  return ( ret_val != 0 ? SIG_ERR : oldact.sa_handler );
}

int sighold(int signo) {

  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, signo);

  return sigprocmask(SIG_BLOCK, &set, NULL);
}

int sigrelse(int signo) {

  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, signo);

  return sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int sigignore(int signo) {

  return ( signal(signo, SIG_IGN) == SIG_ERR ? -1 : 0 );
}

void (*sigset(int signo, void (*disp)(int)))(int) {

  return (disp == SIG_HOLD ?
          (void*)(intptr_t)sighold(signo) : signal(signo,disp));
}

int sigpending(sigset_t *set) {

  struct sigaction act;

  act.sa_handler = SIG_PENDING;

  if ( sigaction(0, &act, &act) != 0 )
    return 1;

  *set = act.sa_mask;

  return 0;
}

int sigaction(int signo, const struct sigaction *act,
              struct sigaction *oldact) {

  Sigact_req_t req;
  Sigact_resp_t resp;
  Msg_status_t status;

  req.type   = resp.type = SC_SIGACT;
  req.signo  = signo;
  req.act    = *act;
  req.sigret = _signal_return;

  req.tag = resp.tag = get_msg_tag();

  msgsend(SYS, &req, sizeof(Sigact_req_t));

  msgrecv(SYS, &resp, sizeof(Sigact_resp_t), &status);

  if ( oldact )
    *oldact = resp.oldact;

  return resp.ret_val;
}

int sigsuspend(const sigset_t *sigmask) {
  
  struct sigaction new;
  
  new.sa_mask = *sigmask;
  new.sa_handler = (__sighandler_t)(intptr_t)SIGSUSP;
  
  return sigaction(SIGSUSP, &new, NULL);
}

/*
 * sigprocmask - block or unblock signals.
 *
 * I route through sigaction because the hoops I need to jump through
 *   for that are much simpler than defining a whole new syscall. Not
 *   to mention that it is desireable to cut down the ways to cross the
 *   user-kernel boundry, even if they're all routed through the same
 *   function.
 */
int sigprocmask(int _how, const sigset_t *_set, sigset_t *_oset) {

  int ret_val;
  struct sigaction new, old;

  /* sanity check */
  if ( _how != SIG_BLOCK   &&
       _how != SIG_UNBLOCK &&
       _how != SIG_SETMASK )
    return 1;

  new.sa_mask    = *_set;
  new.sa_handler = (__sighandler_t)(intptr_t)_how;

  ret_val = sigaction(SIGPMASK, &new, &old);

  if ( _oset )
    *_oset = old.sa_mask;

  return ret_val;
}

/* 
 * special function -> _signal_return 
 *   it is not declared in any header files, so user processes can't 
 *   call it. Instead, its address is passed to the kernel during any 
 *   signal configuration calls. The kernel makes sure that when a 
 *   signal handler calls return, it jumps back here so that the kernel 
 *   can restore the process' context.
 */
int _signal_return(void) {

  Sigret_req_t req;

  req.type = SC_SIGRET;

  msgsend(SYS, &req, sizeof(Sigret_req_t));

  /* no recieve because the msgsend call never returns. Instead, the
     kernel will know that the signal handling code has completed, so
     it will ensure that when control returns to the process, it is
     executing where it was originally interrupted by the signal. */
  return 0;
}
