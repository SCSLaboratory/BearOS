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

#pragma once
/*
 * proc.h
 *
 * Defines the process structure for Bear.
 *
 * The layout of the structure is inspired by MINIX, although updated to run
 * only on the x86-64 platform.
 *
 */

#include <sigcontext.h>
#include <stdint.h>
#include <pci.h>
#include <syscall.h>
#include <signal.h>
#include <kqueue.h>

#define getprocname(p) (p->procnm)
#define setprocname(p,pname) {			\
    kstrncpy(p->procnm,pname,MAX_FNAME_SZ);	\
    p->procnm[MAX_FNAME_SZ-1] = '\0';		\
  }

#define UNREPORTED   0x01
#define STOPPED      0x02
#define CONTINUED    0x04
#define EXITED       0x10
#define SIG_EXITED   0x20
#define SIG_STOPPED  0x40
#define SOME_EXEC_CONSTANT 0x80

#define HEAP_REGION    1
#define STACK_REGION   2
#define TEXT_REGION    3
#define BSS_REGION     4
#define OTHER_REGION  -1

struct memory_region {
  int type;
  int flags;
  uint64_t start;
  uint64_t end;
};

typedef struct _proc {

  /* Machine Context */
  struct mcontext mc;         /* THIS MUST COME FIRST! (machine regs)       */
  struct mcontext kmc;        /* MUST BE SECOND! kernel machine context
                                 for lateral move to next process   */
  /* THIS MUST BE THIRD */
  struct page_map_level_4_table *cr3_target;   /* physical location of processes PML4T */
  
  /* Privilege Info */
  pid_t pid;                  /* Process ID                                 */
  uint8_t pl;                 /* Privilege Level                            */

  /* Paging and Memory */
  /* TODO: erase these 2 */
  union pt_entry *pml4t_entry;
  struct page_directory_pointer_table *pdpt;
  
  /* virtual memory spaces */
  queue_t *mapped_memory_regions;
  struct memory_region *heap_region;

  /* argv/argc + environ shtuff */
  int argc;
  char **argv;
  int envc;
  uint64_t env;

  /* Message-Passing */
  Message_t *recvp;           /* Userland addr of message buffer            */
  pid_t recvfrom;             /* PID from whence the message will come      */
  
  /* Process Genealogy */
  struct _proc * parent;      /* Pointer to parent Proc_t                   */
  void *childrenq;            /* Child processes that are still running     */
  void *zombieq;              /* Child processes that have exited           */
  int waiting_for;            /* if proc is wait()'ing, it's on this pid    */
  int status;	              /* process status as listed in defines above  */

  /* temporary... the search function only gets passed the proc struct and 
     a potential message to check if its the right one. Right before searching 
     is when I'll check the tag */
  unsigned int search_tag;

  /* this func is called at end of wait   */
  void (*cb_func)(struct _proc*,int,int,unsigned int);

  /* Used during refresh */
  Pci_dev_t *dev;           /* What dev is this proc servicing? (drivers)  */
  uint64_t paddr;           /* true paddr of the DMA space.                */
  
  /* Signal handling stuff */
  sigset_t sigignore;     /* 1 means ignore the signal, 0 means don't      */
  sigset_t sigcatch;      /* 1 means catch the signal, 0 means don't       */
  sigset_t sigmask;       /* signals to be blocked                         */
  sigset_t sigmask2;      /* saved copy of a signal mask                   */
  sigset_t sigpending;    /* signals being blocked                         */
  sigset_t sigdispatch;   /* signals that were pending and then unblocked  */
  int (*sigret)(void);    /* address of the special signal return function */
  ucontext_t *ucp;        /* pointer to context saved by current signal    */
  struct sigaction sigact[_NSIG]; /* array of actions for signals          */

  /* Stuff saved from the ELF file, including the file handle. */
  struct elf_ctx *file;      /* Dynamically allocated ELF information obj  */
  uint64_t allocated_pages;  /* number of user level pages this proc owns  */

  /* (ST) quick hack to be able to get the process name (should be available
   * the elf_ctx somewhere
   */
  char procnm[MAX_FNAME_SZ];
  int stdio[3]; /* default stdio daemons */
  int is_in_mem; /* Whether or not the binary came over the network */

  uint64_t pes_hack_kernel;
  uint64_t pes_hack_user;
} __attribute__ ((packed)) Proc_t;

typedef struct _Zombie {
  int parentid;
  int status;
  int pid;
  char procnm[MAX_FNAME_SZ];
} Zombie;

