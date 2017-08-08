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

/******************************************************************************
 * Filename: ksched.c
 *
 * Description: 
 * This file implements the scheduler for the Bear microkernel.
 *
 *****************************************************************************/

#include <constants.h>
#include <kernel.h>
#include <proc.h>
#include <procman.h>
#include <kqueue.h>
#include <sbin/syspid.h>
#include <ksched.h>
#include <kstring.h>
#include <syscall.h>
#include <smp.h>
#include <kmalloc.h>
#include <interrupts.h>
#include <khash.h>

extern void idle(uint64_t*);          /* asm func to make CPU idle           */

/*** PRIVATE DECLARACTIONS ***/

/* Private vars */
static void *readyq;                  /* The queue of ready-to-run procs */
static void *hookq;                   /* Hooks to run before scheduling */


/* Private functions */
static void ksched_set_next(Proc_t *p);
static int hooksearch(void *ep,const void *kp);
static void hookapply(void *procp,void *hookp);
static void ksched_printproc(void *resp, void *vp);
static void ksched_printchild(void *ep);
static void ksched_printzomid(void *ep);

/*** PUBLIC FUNCTIONS ***/
void ksched_printrelations(Proc_t *p);
void ksched_save_entry(char status,Ps_resp_t *resp,Proc_t* p);

/* Initialize the shcedule module. */
int ksched_init() {

  int i, rc;

  /* Init run queue */
  readyq = qopen();


  /* Initialize hook queue. */
  hookq = qopen();

  /* Create idle proc for when no other processes are ready to run */
  idleps = kmalloc_track(KSCHED_SITE, smp_num_cpus*sizeof(Proc_t*));
  for ( i = 0; i < smp_num_cpus; i++ ) {
#ifdef DEBUG_SMP    
    kprintf("initializing idle proc %d\n", i);
#endif
    idleps[i] = new_proc((uint64_t)(&idle), PL_0, IDLE_PROC, NULL);
    setprocname(idleps[i],"idle"); 
  }

  /* We don't have anything to run yet. */
  proc_ptr_array = (Proc_t**)kmalloc_track(KSCHED_SITE, smp_num_cpus*sizeof(Proc_t*));
  kmemset(proc_ptr_array, 0, smp_num_cpus*sizeof(Proc_t*));


  return 0;
}

/* Add a hook function to be run with the sceduler. */
void ksched_hook_add(ksched_hook hook) {
  qput(hookq, hook);
}

void ksched_hook_remove(ksched_hook hook) {
  qremove(hookq, hooksearch, hook);
}

/*
 * Assuming the kernel is running in some sort of interrupt/syscall handler,
 * this function will return the function that was running when the
 * interrupt/syscall happened. Thus, it's the process that made the syscall
 * or was running when the CPU core received the interrupt.
 */
Proc_t *ksched_get_last() {

#ifndef ENABLE_SMP
  return *proc_ptr_array ? *proc_ptr_array : idleps[0];
#else
  return *(proc_ptr_array + this_cpu()) ? *(proc_ptr_array + this_cpu()) : idleps[this_cpu()];
#endif
}

/*
 * Run the scheduler.  Call this LAST, just before returning to a user process.
 * The output of this function is the next process to run.
 * This function updates the "last process" state variable in anticipation of
 *  the next interrupt/kernel-invocation.  So you'd better RUN IT LAST;
 *  make sure nobody calls ksched_get_last() after it's run.
 */
Proc_t *ksched_schedule() {

  Proc_t *next;  /* Next process to run   */

  /* Find a replacement, or idle til interrupt */
  
  next = (Proc_t *)qget(readyq);

  if ( next ) 
    qapply2(hookq, next, hookapply);

  /* Update our state variable indicating what is about to run. */
  ksched_set_next(next);

  if(next                   && 
     next->pid != IDLE_PROC && 
     next->pid != -666      && 
     next->pid != -80085    && 
     next->zombieq==NULL) {
    kprintf("Something went seriously wrong\n");
    kprintf("next->pid = %d proc name %s\n", next->pid, next->procnm);    
    asm volatile("hlt");
  }
  
  return ksched_get_last();
}

/* 
 * FIXME: This will cause ksched_get_last() to return invalid NULL,
 *  so ksched_get_last() better not get called after this function
 *  is called.
 */
void ksched_block(Proc_t *p) {

  qremove(readyq, &is_process, (void*)(&(p->pid)));

  update_proc_status(p,0,STOPPED);
}

/* Called when a process is able to run again. */
void ksched_unblock(Proc_t *p) {
  qput(readyq, (void*)p);

  update_proc_status(p,0,CONTINUED);
}

/* Should be called when a new process is created */
void ksched_add(Proc_t *p) {

  /* todo, check for and skip idle procs in here. */

    qput(readyq, (void*)p);
}


/* 
 * FIXME: This will cause ksched_get_last() to return invalid NULL,
 *  so ksched_get_last() better not get called after this function
 *  is called.
 */
void ksched_purge(Proc_t *p) {
  qremove(readyq, &is_process,(void*)(&(p->pid)));
}

void ksched_ps(void *resp) {

  happly2(get_proc_lut(), resp, ksched_printproc);

  return;
}

/*** PRIVATE DECLARATIONS ***/

/*
 * Update the state variable that tracks what process was running when the
 * last interrupt/syscall happened.  This should be called just before the
 * kernel exits and gives control to a user process, so that when the next
 * interrupt/syscall occurs this variable is consistent with what was just
 * running.
 */
static void ksched_set_next(Proc_t *p) {
#ifdef ENABLE_SMP
  *(proc_ptr_array + this_cpu()) = p;
#else
  *proc_ptr_array = p;
#endif
}

/* Search for a hook in the hook queue. Search is the function address. */
static int hooksearch(void *ep, const void *kp) {
  ksched_hook element = (ksched_hook)ep;
  ksched_hook key = (ksched_hook)kp;
  return element == key;
}

static void hookapply(void *procp, void *hookp) {
  Proc_t *proc=(Proc_t*)procp;
  ksched_hook hook=(ksched_hook)hookp;
  hook(proc);
}

#define TABSTOP 8

/* ksched_printproc -- prints the scheduing queue */
static void ksched_printproc(void *resp, void *vp) {

  Proc_t *p = (Proc_t *)vp;
  Ps_resp_t *rp;
  char status;
  char str[2];

  if(p->status & (STOPPED | SIG_STOPPED))
    status = 'S';
  else if(p->status & CONTINUED)
    status = 'R';
  else if(p->status & (EXITED | SIG_EXITED))
    status = 'Z';
  else
    status = 'F';

  if(p == ksched_get_last())
    status = 'R';
  str[0] = status;
  str[1] = '\0';

  rp=(Ps_resp_t*)resp;
  if(rp) 
    ksched_save_entry(status,rp,p);
  if(kstrlen(p->procnm)>=TABSTOP)	/* use one less tab */
    kprintf("%s  %d\t%s\t",str,p->pid,p->procnm);
  else
    kprintf("%s  %d\t%s\t\t",str,p->pid,p->procnm);
  ksched_printrelations(p);
}

void ksched_save_entry(char status,Ps_resp_t *rp,Proc_t* p) {
  int nextentry;

  if(rp->entries<MAX_PS_SZ) {
    nextentry = rp->entries;
    rp->status[nextentry] = status;
    rp->pid[nextentry] = p->pid;
    kstrncpy(&rp->procnm[nextentry][0],p->procnm,MAX_FNAME_SZ);
    rp->entries++;
  }
}

void ksched_printrelations(Proc_t *p) {
  Proc_t *parentp;
  parentp = p->parent;
  if(parentp==NULL)
    kprintf("sys\t");      
  else if(parentp->pid == SYSD) /* root */
    kprintf("sysd\t");
  else
    kprintf("%d\t",parentp->pid); 
  qapply(p->childrenq, ksched_printchild);
  kprintf(" ; ");
  qapply(p->zombieq, ksched_printzomid);

  kprintf("\n");     
}

static void ksched_printzomid(void *ep) {
  Zombie *p;
  p=(Zombie*)ep;
  kprintf(":%d",p->pid);
}

static void ksched_printchild(void *ep) {
  Proc_t *p;
  p=(Proc_t*)ep;
  kprintf(":%d",p->pid);
}

/*don't change this function*/
void ksched_yield(/**DO NOT ADD ARGUMENTS*/){

  Proc_t *curr_p;
  Proc_t *next_p;

  /* Must occur first to save context correctly if
   * information present in general purpose registers
   */
  save_kernel_context(); 

  curr_p = ksched_get_last();

  next_p = ksched_schedule();

  curr_p->kmc.rip = *(uint64_t*)((uint64_t)curr_p->kmc.rbp+8);
  curr_p->kmc.rsp = (reg_t)((uint64_t)curr_p->kmc.rbp+16);
  curr_p->kmc.rbp = *(uint64_t*)((uint64_t)curr_p->kmc.rbp); 

  restore_kernel_proc(next_p);
  kpanic("[KERNEL] Panic - Yield should not reach this!");
}

int truefun(void *e, const void* s) {
  return 1;
}

Proc_t *ksched_get_next(void) {

  return qsearch(readyq, truefun, 0x0);
}
