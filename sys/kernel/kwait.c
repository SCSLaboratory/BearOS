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
 * Filename: kwait.c
 *
 * Description:
 *  Implements the wait() and waitpid() syscalls.
 *
 *****************************************************************************/

#include <stdint.h>
#include <kqueue.h>
#include <kmalloc.h>
#include <proc.h>
#include <procman.h>
#include <kmsg.h>
#include <ksched.h>
#include <kwait.h>
#include <kernel.h>
#include <kstdio.h>
#include <kvmem.h>
#include <sys/wait.h>
#include <kstring.h>

/******************************************************************************
 **************************** PRIVATE DECLARATIONS ****************************
 *****************************************************************************/

static int  zombie_pid_cmp(void*, const void*);  /* Compare pid with zombie */
static int  proc_pid_cmp(void*, const void*);    /* Compare pid with proc   */
static void set_orphan(void*);	/* Sets the proc's parent to NULL   */

static void reap(void);

static queue_t *reaping_q;
static Proc_t *grim_reaper;

/******************************************************************************
 ****************************** PUBLIC FUNCTIONS ******************************
 *****************************************************************************/

/* init function */
int kwait_init() {
  
  reaping_q = qopen();
  grim_reaper = new_kernel_proc((uint64_t)reap, -666);
  
  return 0;
}  

/* Called whenever a new proc is made */
void kwait_new(pid_t child_pid, pid_t parent_pid) {
  Proc_t *child;  /* Child proc  */
  Proc_t *parent; /* Parent proc */

  child  = pid_to_addr(child_pid);
  if(parent_pid)
    parent = pid_to_addr(parent_pid);
  else
    parent = NULL;

  child->parent = parent;
  child->childrenq = qopen();
  child->zombieq = qopen();
  if (parent != NULL) {
    qput(parent->childrenq, child);
  }
}

/* Action of the wait syscall. */
int kwait_wait(pid_t waiter_pid, pid_t pidarg, int opts,
	       void (*callback_func)(Proc_t*,int,int,unsigned int), unsigned int tag) {

  Proc_t *waiter; /* Process that will be suspended */
  Proc_t *child;  /* Process to wait for            */
  Zombie *zom;    /* Zombie process                 */

  waiter = pid_to_addr(waiter_pid);

  /* Is there a child waiting to be collected? At this point, pidarg != 0:
   *    pidarg  >  0 means pidarg is pid of a specific process to wait for
   *    pidarg == -1 means wait for any child
   *    pidarg  < -1 means wait for any child whose process group = -pidarg
   */
  child = qsearch(waiter->childrenq, proc_pid_cmp, &pidarg);
  if (child != NULL) {

    /* To be used when we get group process ids
       if ( pidarg < -1 && -pidarg != curr->gid )
       continue;
    */
    /* is there any unreported state change? */
    if ( child->status & UNREPORTED ) {
	
      if ( WIFCONTINUED(child->status) && (opts & WCONTINUED) ) {
	update_proc_status(child,0,0);
	callback_func(waiter, child->pid, child->status, tag);
	return 0;
      }
	
      if ( WIFSTOPPED(child->status)   && (opts & WUNTRACED) ) {
	update_proc_status(child,0,0);
	callback_func(waiter, child->pid, child->status, tag);
	return 0;
      }
	
    } /* end UNREPORTED */
  } 

  /* Search zombie queue for already-exited child process */
  zom = qremove(waiter->zombieq, zombie_pid_cmp, &pidarg);
  if (zom != NULL) {
    callback_func(waiter,zom->pid,zom->status,tag); /* sends the response */
    kfree_track(KWAIT_SITE,zom);
    return 0;
  }
  /* No suitable zombie; let's see if there's a suitable running child */
  child = qsearch(waiter->childrenq, proc_pid_cmp, &pidarg);
  if (child != NULL) {
    if ( opts & WNOHANG )
      callback_func(waiter, 0, 0,tag); 
    else {
      waiter->search_tag = tag;
      waiter->waiting_for = pidarg;
      waiter->cb_func = callback_func;   /* Save cb_func to call later.     */
    }
    return 0;                          /* Suspend til child proc is done. */
  }

  /* waiter has not spawned a child with pid == wpid.  Error. */
  return -1;
}

/* Called whenever an exit happens */
void kwait_exit(Proc_t *p) {
  Proc_t *parent, *sysdp;  /* The parent of proc p that is exiting */

  parent = p->parent;		/* find parent of process p */

  /* Restart the parent, if parent is waiting on this process. */
  if (parent != NULL) {
    /* Take p out of childrenq of parent */
    qremove(parent->childrenq,proc_pid_cmp,&(p->pid));
    /* If parent is waiting for p, wake it */
    if ((parent->waiting_for == ANY) || (parent->waiting_for == p->pid)) {
      parent->waiting_for = 0;
      (parent->cb_func)(parent,p->pid, p->status, parent->search_tag);
    }
    else {			/* Else, make a zombie */
      Zombie *z = kmalloc_track(KWAIT_SITE,sizeof(Zombie));
      kstrncpy(z->procnm,p->procnm,MAX_FNAME_SZ);
      z->parentid = parent->pid;
      z->status = p->status;
      z->pid = p->pid;
      qput(parent->zombieq,z);
    }
  }
  else
    kprintf("child %d parent was null\n", p->pid);
  
  /* Free the zombies */
  if (p->zombieq == NULL)
    kpanic("Corrrupted zombie queue!");
  else
    qclose(p->zombieq);		/* free all the zombies */

  /* Move children to sysd */
  if(p->childrenq == NULL)
    kpanic("Corrupted children queue!");
  qapply(p->childrenq,set_orphan);  /* make child parent be sysd */
  
  sysdp = pid_to_addr(SYSD);	    /* add children to sysd  */
  qconcat(sysdp->childrenq,p->childrenq);

  reap_proc(p); 
  return;
}

void reap_proc( Proc_t *p ) {

  qput(reaping_q, p);
  ksched_unblock(grim_reaper);

  return;
}

/* Called whenever an exit happens */
void kwait_assume(Proc_t *p) {
  Proc_t *parent;  /* The parent of proc p that is exiting */

  parent = p->parent;		/* find parent of process p */

  if (parent != NULL) {
    /* Take p out of childrenq of parent */
    qremove(parent->childrenq,proc_pid_cmp,&(p->pid));
  }
  else
    kprintf("child %d parent was null\n", p->pid);

  /* Free the zombies */
  if (p->zombieq == NULL)
    kpanic("Corrrupted zombie queue!");
  else
    qclose(p->zombieq);		/* free all the zombies */

  qclose(p->childrenq);

    reap_proc(p); 
 
  return;
}

static void reap(void) {

  Proc_t *victim;

  int i;
  
  int pml4t_idx;
  int pdpt_idx;
  int pd_idx;
  int pt_idx;

  union pt_entry pml4te, pdpte, pde;
  union page pte;

  uint64_t special_k, special_k_base;
  uint64_t pml4t_virt;
  uint64_t pdpt_virt;
  uint64_t pd_virt;
  uint64_t pt_virt;

  while ( 1 ) {

    victim = (Proc_t*)qget(reaping_q);
    while ( victim ) {

      flush_tlb(TLB_ALL);

      /* find a spare pml4t entry in the reaper's tables */
      for ( i = 0; i < 512; i++ )
	if ( !((union pt_entry*)PML4TE2vaddr(i))->present )
	  break;
      
      /* check if we found a free one */
      if ( i == 512 ) 
	kpanic("NOT ENOUGH SPACE IN THE PML4T!!\n");

      special_k_base = special_k = (uint64_t)idx2vaddr(i, 0, 0, 0);

      pml4t_virt = special_k;
      attach_page(special_k, (uint64_t)victim->cr3_target, PG_RW);
      special_k += PAGE_SIZE;
      
      for ( pml4t_idx = 0; pml4t_idx < 512; pml4t_idx++ ) {
	  
	if ( pml4t_idx == PML4T_RECURSIVE_ENTRY_IDX ) 
	  continue;
	
	pml4te.bits = ((union pt_entry*)(pml4t_virt + pml4t_idx*sizeof(union pt_entry)))->bits;
	
	if ( !pml4te.present )
	  continue;
	
	pdpt_virt = special_k;
	attach_page(special_k, (uint64_t)TABLE2ADDR(pml4te.addr), PG_RW);
	special_k += PAGE_SIZE;
	
	for ( pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++ ) {
	  
	  pdpte.bits = ((union pt_entry*)(pdpt_virt + pdpt_idx*sizeof(union pt_entry)))->bits;
	  
	  if ( !pdpte.present )
	    continue;

	  pd_virt = special_k;
	  attach_page(special_k, (uint64_t)TABLE2ADDR(pdpte.addr), PG_RW);
	  special_k += PAGE_SIZE;
	  
	  for ( pd_idx = 0; pd_idx < 512; pd_idx++ ) {
	    
	    pde.bits = ((union pt_entry*)(pd_virt + pd_idx*sizeof(union pt_entry)))->bits;
	    
	    if ( !pde.present )
	      continue;
	    
	    pt_virt = special_k;
	    attach_page(special_k, (uint64_t)TABLE2ADDR(pde.addr), PG_RW);
	    special_k += PAGE_SIZE;
	    
	    for ( pt_idx = 0; pt_idx < 512; pt_idx++ ) {
	      
	      pte.bits = ((union page*)(pt_virt + pt_idx*sizeof(union pt_entry)))->bits;
	      
	      if ( !pte.present ) 
		continue;
	      
	      /* don't free supervisor (kernel) pages */
	      if ( pte.global )
		continue;

	      attach_page(special_k, (uint64_t)TABLE2ADDR(pte.addr), PG_RW);
	      special_k += PAGE_SIZE;
	    } /* end pt loop */
	  } /* end pd loop */
	} /* end pdpt loop */
      } /* end pml4t loop */

      vmem_free((uint64_t*)special_k_base, special_k - special_k_base);	

      /* free kmalloced proc structure */
      kfree_track(KWAIT_SITE, victim);
      
      /* if I am to reap another victim, I need to flush the TLB */
      victim = (Proc_t*)qget(reaping_q);
      if ( victim )
      	flush_tlb(TLB_ALL);
    }

    ksched_yield();
  }

  kpanic("The Grim Reaper tried to return!");
}

/*
 * PRIVATE FUNCTIONS
 */

/*
 * zombie_pid_cmp() - Returns 1 if the passed-in Zombie has the given
 * pid value, or if the pid value is ANY.
 */
static int zombie_pid_cmp(void *zp, const void *pidp) {
  Zombie *z = zp;
  int *pid = (int*)pidp; 

  if (z == NULL)
    kpanic("Null child given to zombie_pid_cmp.");
  if (pid == NULL)
    kpanic("Null pid pointer given to zombie_pid_cmp.");
  return ((*pid == ANY) || (*pid == z->pid));
}

/*
 * Function: proc_pid_cmp() - returns 1 if the passed-in Proc_t has
 * the given pid value, or if the pid value is ANY.
 */
static int proc_pid_cmp(void *pp, const void *pidp) {
  Proc_t *p = (Proc_t *)pp;
  int *pid = (int *)pidp;
  if (p == NULL)
    kpanic("Null child given to proc_pid_cmp.");
  if (pid == NULL)
    kpanic("Null pid pointer given to proc_pid_cmp.");
  return ((*pid == ANY) || (*pid == p->pid));
}

/*
 * Function: set_orphan() -- make a childs parent the init process
 */
static void set_orphan(void *cp) {
  Proc_t *childp, *sysdp;

  childp = (Proc_t*)cp;
  sysdp = pid_to_addr(SYSD);
  if (childp == NULL || sysdp == NULL)
    kpanic("Null used to orphan!");
  childp->parent = sysdp;
}
