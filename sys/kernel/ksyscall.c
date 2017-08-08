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
 * ksyscall.c -- Implements the core system calls provided by the kernel.
 *
 */
#include <kstdio.h>
#include <ksyscall.h>
#include <constants.h>
#include <kvmem.h>              /* For paging flags and MMIO calls */
#include <procman.h>
#include <kmalloc.h>
#include <kqueue.h>
#include <kmsg.h>
#include <kernel.h>
#include <ktimer.h>
#include <kstring.h>
#include <ktime.h>
#include <syscall.h>
#include <fatfs.h>
#include <kwait.h>
#include <pio.h>
#include <msg.h>
#include <kmsg.h>
#include <msg_types.h>
#include <network.h>
#include <ksched.h>
#include <interrupts.h>
#include <asm_subroutines.h>
#include <ktime.h>
#include <memory.h>
#include <elf_loader.h>
#include <proc.h>
#include <signal.h>
#include <sigcontext.h>
#include <kvcall.h>
#include <apic.h>
#include <interrupts.h>

#include <sys/wait.h>

#include <sbin/vgad.h>
#include <sbin/kbd.h>
#include <sbin/lwip/netif.h>

/* helper functions used by signaling sys_do_xxx */
static int _do_sigaction(Proc_t*, int, struct sigaction *);
static int  _do_kill(Proc_t*, int);

/* wakes a process after sleeping */
static void systask_unsleep(void*);        

/* Wakes proc after waiting    */ 
static void systask_unwait(Proc_t*, int, int,unsigned int); 

/*
 * systask_msgsend() - Invokes the user-process send mechanism,
 * allowing the systask to send a message to a user process.  This is
 * a blocking call.  Additionally, because this call invokes the
 * kernel, the systask CANNOT be in the middle of an atomic operation
 * on a kernel data structure.
 */
static inline int systask_msgsend(int dst, void *m, int sz) {
  /*
   * 20131212 JMD: Converted systask handlers to operate in kmsg handler...
   * so we can now use kmsg_send 
   */
  Message_t msg;
  /* NOTE: must force the src to SYS since we no longer trap through the */
  /* interrupt handler, where this is normally set by kmsg_handle_syscall() */
  msg.src = SYS; 
  msg.dst = dst;
  msg.len = sz;
  msg.buf = m;
  msg.status = NULL;

  kmsg_send(&msg);
  
  return 0;
}

/******************************************************************************
 *************************** SYSCALL SERVICE ROUTINES *************************
 *****************************************************************************/

/*
 * systask_do_fork(Proc_t*, Message_t*)
 */
void systask_do_fork(Systask_msg_t *msg, Msg_status_t *status) {
  Fork_resp_t resp;    /* Response message    */
  Proc_t *np;          /* New proc            */
  Proc_t *parent;      /* Parent proc         */

  parent = pid_to_addr(status->src);

  /* Clone proc structures */
  np = clone_proc(parent);
  if(np == NULL) {
    kpanic("[KERNEL] - Warning fork failed");
  }

  /* Is the proc waiting on a msgrecv? */
  if (np->recvp == NULL) {/* Not waiting on a msgrecv */
    ksched_add(np);
  } 
  else {/* Waiting on a msgrecv     */
    int ret;

    ret = kmsg_recv(np,np->recvp, np->recvfrom);
    if (ret)
      ksched_unblock(np);
  }

  resp.type = SC_FORK;

  /* start the parent */
  resp.ret = np->pid;
  systask_msgsend(parent->pid, &resp, sizeof(Fork_resp_t));


  /* start the child */
  resp.ret  = 0;
  systask_msgsend(np->pid, &resp, sizeof(Fork_resp_t));
}

/*
 * systask_do_exec(Proc_t*, Message_t*)
 *
 * Perform the exec syscall.  In this implementation, everything is
 * wiped out except pid, privilege info, and plut_next.
 */

#define TEMP_PID -80085

void systask_do_exec(Systask_msg_t *msg, Msg_status_t *status) {
  Exec_req_t *req;
  Exec_resp_t resp;
  Proc_t *p, *np;
  int i;

  char fname[MAX_FNAME_SZ];

  p = pid_to_addr(status->src);	/* p points to copy of parent */
  req = (Exec_req_t *)msg;
  resp.type = SC_EXEC;

  /* Race in elf_load at end? req->fname not valid when we reach there... */
  kstrncpy(fname, req->fname, MAX_FNAME_SZ);

  FILINFO ramstatus;
  /* go to the ram disk */
  if(f_stat(fname,&ramstatus)!=0) {
    resp.type = SC_EXEC;
    resp.ret = -1;
    systask_msgsend(p->pid, &resp, sizeof(Exec_resp_t));
    return;
  }


  np = new_proc(USR_TEXT_START, p->pl, TEMP_PID, p->parent);
  np->argc = req->argc;

  np->argv = (char**)kmalloc_track(SYS_TASK_SITE, sizeof(char*)*np->argc); 
  for ( i = 0; req->argv[i]; i++ ) {
    np->argv[i] = (char*)kmalloc_track(SYS_TASK_SITE, (sizeof(char)*kstrlen(req->argv[i])) + 1);
    kstrncpy(np->argv[i], req->argv[i], kstrlen(req->argv[i]) + 1);
  }

  np->envc = req->num_env;
  np->env = (uint64_t)kmalloc_track(SYS_TASK_SITE, PAGE_SIZE);
  kmemset((void*)np->env, 0, PAGE_SIZE);

  uint64_t ptr = np->env + sizeof(char *)*(np->envc + 1);

  for ( i = 0; i < np->envc; i++ ) {
    ((char**)np->env)[i] = (char*)ptr;
    kstrncpy((char*)ptr, req->env[i], kstrlen(req->env[i]) + 1);
    ptr += kstrlen(req->env[i]) + 1;
  }

  for(i=0; i<3; i++)
    np->stdio[i]=p->stdio[i];

  destroy_proc(p, SOME_EXEC_CONSTANT);

  lut_remove(TEMP_PID);
  np->pid = p->pid;
  lut_add(np);

  /* Load the code in, and diversify if necessary 
   * Afterwards, we setup the ARGV and ENVIRON
   */
  np->is_in_mem = 0;

  setprocname(np, fname);

  /* reset signal dispositions */
  sigemptyset(&np->sigmask);
  sigemptyset(&np->sigcatch);

  /* Reset message passing */
  np->recvp = NULL;
  np->recvfrom = PROC_NONE;

  ksched_unblock(np);

  ksched_yield();
}

/*
 * systask_do_getpid(Proc_t*, Message_t*)
 */
void systask_do_getpid(Systask_msg_t *msg, Msg_status_t *status) {
  Proc_t	*user_proc;
  Getpid_resp_t resp;
  resp.type = SC_GETPID;
  user_proc = ksched_get_last();

  /*
   * this is for signaling - the keyboard is a user process and we want to 
   * kill the last spawned non background process (RMD)
   */
  if(user_proc->pid == KBD){	/* last process did ^c  */
    resp.pid = *user_pid_counter;
    resp.pid--;
  }
  else
    resp.pid  = status->src;

  systask_msgsend(status->src, &resp, sizeof(Getpid_resp_t));
}

/*
 * systask_do_umalloc(Proc_t*, Message_t*)
 */
void systask_do_umalloc(Systask_msg_t *msg, Msg_status_t *status) {
  
  Umalloc_req_t *req;
  Umalloc_resp_t resp;
  Proc_t *p;
  uint64_t size, num_pages;

  req = (Umalloc_req_t *)msg;
  p = pid_to_addr(status->src);
  resp.type = SC_UMALLOC;

  /* Extract size and convert to pages */
  size = req->bytes;
  if(size % PAGE_SIZE)
    size += PAGE_SIZE - (size % PAGE_SIZE);
  num_pages = size / PAGE_SIZE; 

  vmem_alloc( (uint64_t*)p->heap_region->end, num_pages*PAGE_SIZE, 
	      PG_USER | PG_RW | PG_NX );

  resp.addr = p->heap_region->end;
  p->heap_region->end += num_pages*PAGE_SIZE;
  resp.ret_val = EXIT_SUCCESS;

  systask_msgsend(status->src, &resp, sizeof(Umalloc_resp_t));

  return;
}

/*
 * systask_do_waitpid(Proc_t*, Message_t*)
 */
void systask_do_waitpid(Systask_msg_t *msg, Msg_status_t *status) {
  Waitpid_req_t *req;   /* Incoming request           */
  Waitpid_resp_t resp;  /* Outgoing response          */
  int waiter_pid;       /* PID of process to suspend  */
  int child_pid;        /* PID of process to wait for */
  int opts, rc;

  req = (Waitpid_req_t *)msg;
  waiter_pid = status->src;
  child_pid  = req->wpid;
  opts       = req->opts;
  resp.tag = req->tag;

  /* Wait until proc with PID==wpid has exited */
  rc = kwait_wait(waiter_pid, child_pid, opts, systask_unwait, req->tag);
  
  if (rc == -1) { /* error;  p has no child with PID == wpid. */
    kprintf("Error: Wait attempted on nonexistent child.\n");
    resp.type     = SC_WAITPID;
    resp.exit_val = 0;
    resp.wpid     = -1;
    systask_msgsend(waiter_pid, &resp, sizeof(Waitpid_resp_t));
  }

  /* waiter_pid will be woken up when the specified child exits. */
}

/*
 * systask_do_kill(Proc_t*, Message_t*) -- signal a process
 *
 * despite the somewhat deceptive name, this is used to signal a
 * process. This signal might be a "kill", but it need not be.
 * the naming convention is consistent wth other *NIX systems.
 */
void systask_do_kill(Systask_msg_t *msg, Msg_status_t *status) {

  Kill_req_t *req;  /* Incoming signal request           */
  Kill_resp_t resp;
  Proc_t *p;        /* Process that is exiting           */
  int signo;        /* signal number that was sent       */

  /* decode message */
  req   = (Kill_req_t *)msg;

  signo = req->kill_sig;
  p     = pid_to_addr(req->kill_pid);

  /* build response */
  resp.type    = SC_KILL;
  resp.tag = req->tag;
  resp.ret_val = ( _do_kill(p, signo) < 0 ? -1 : 0 );

  /* send response to calling proc */
  systask_msgsend(status->src, &resp, sizeof(Kill_resp_t));

  return;
}

/*
 * systask_do_exit(Proc_t*, Message_t*) - exit
 */
void systask_do_exit(Systask_msg_t *msg, Msg_status_t *status) {
  Exit_req_t *req;  /* Incoming exit request   */
  Proc_t *p;        /* Process that is exiting */

  req = (Exit_req_t *)msg;
  p = pid_to_addr(status->src);

  _do_kill(p->parent, SIGCHLD);

  destroy_proc(p,req->exit_sig); /* free everything */

  ksched_yield();
}

/*
 * systask_do_usleep(Proc_t*, Message_t*)
 */
void systask_do_usleep(Systask_msg_t *msg, Msg_status_t *status) {
  Usleep_req_t *req;   /* Incoming usleep request  */
  Usleep_resp_t resp;  /* Outgoing usleep response */
  Proc_t *p;           /* Proc to be put to sleep  */
  int ms;
  
  req = (Usleep_req_t *)msg;
  p = pid_to_addr(status->src);
  ms = req->slp_ms;
  
  /* Sanity check */
  if (ms <= 0) {
    resp.type = SC_USLEEP;
    /* TODO: This msgsend was calling a userspace functoin... such a massive 
       fail... need to call kmsgsend... don't want to change & test it right 
       now though... -SLB 1/16/17 */
    //    msgsend(status->src, &resp, sizeof(Usleep_resp_t));
    return;
  }

  /* Set an alarm to call unsleep after ms milliseconds */
  ktimer_new_alarm(ms,systask_unsleep,(void*)p);

  /* No message back from here; callback will send the message after wait */
}

/*
 * systask_do_map_vga_mem(Proc_t*, Message_t*) -- Maps the VGA display
 * memory into the requesting process's address space.  The requesting
 * process had BETTER be the display driver.
 */
void systask_do_map_vga_mem(Systask_msg_t *msg, Msg_status_t *status) {
  Vgamem_resp_t resp;
  Proc_t *src;  /* Proc that made the syscall */
  src = pid_to_addr(status->src);
  if (src == NULL) {
    kprintf("%s: Could not find proc of sender!\n",__FUNCTION__);
    return;
  }
  
  resp.type = SC_VGAMEM;
  
  
  if (src->pid != VGAD) { 	     /* Make sure this is the VGA driver. */
    kprintf("NOT the vgad.\n");
    resp.vaddr = (uint16_t *)NULL;
  } else { 	   /* VGA phys mem gets mapped to designated driver space */
    Pci_bar_t bar; /* Have to put one together for the mapping function. */

    /* 
     * I'm cheating.  I know that kvmem_map_mmio() only looks at bar_base
     * and bar_size.  bar_limit is broken, I think.
     * The actual amount of memory used by the VGA hardware depends on 
     * what mode it's in, I think.  One page is enough for what we're doing.
     */
    bar.bar_base = PHYS_VGA_MEM;
    bar.bar_size = PAGE_SIZE;
    kvmem_map_mmio(src, DRIVER_MEM_START, &bar);
    resp.vaddr = (uint16_t*)DRIVER_MEM_START;
  }
  
  systask_msgsend(src->pid, &resp, sizeof(Vgamem_resp_t));
}

/*
 * systask_do_reboot(Message_t*)
 */
void systask_do_reboot(Systask_msg_t *msg, Msg_status_t *status) {
  unsigned char good = 0x2;
  
  /* 
   * Code from osdev for restarting machine - waits for "ok" from keyboard
   * hardware, then sends the "reset" request.  Yes that's right the
   * legacy keyboard hardware is in charge of restarting the machine
   * (makes sense if you think about it... )
   * I believe this behavior actually emulated by the BIOS or mobo.  Power
   * on modern day machines is handled thru ACPI.
   */
  while ((good & 0x2) != 0)
    good = inb(0x64);
  outb(0x64, 0xFE);
  asm volatile("hlt");
  
  /* Not reached */
}

/*
 * systask_do_unmask_irq(Message_t*) -- Allow an interrupt to fire again.
 */
void systask_do_unmask_irq(Systask_msg_t *msg, Msg_status_t *status) {
  Unmask_msg_t *umsg;
  umsg = (Unmask_msg_t *)msg;

  ioapicenable(umsg->irq, 0); /*enables the network interrupt*/

  lapic_eoi();

}

void systask_do_eoi(Systask_msg_t *msg, Msg_status_t *status) {
  lapic_eoi();
}

#ifdef KERNEL_DEBUG
void systask_do_print_addr(Systask_msg_t *msg, Msg_status_t *status){

  kprintaddr_req_t *req;
  int i, z ;
  req = (kprintaddr_req_t *)msg;

  kprintf("address to print is %x \n", req->addr);
		
  for(i =0, z =1 ; i < req->bytes; i++, z++ ){
    kprintf("%x ", *(uint8_t *)((req->addr) + i));

    if(!(z%20))	
      kprintf("\n");
  }	

  kprintf("%x \n", *(uint64_t *)PTE2vaddr(virt2pml4t(req->addr), virt2pdpt(req->addr), virt2pd(req->addr), virt2pt(req->addr) ));
				
}
#endif

/*
 * systask_do_get_core(Message_t*) -- returns the core number to the user process
 */
void systask_do_get_core(Systask_msg_t *msg, Msg_status_t *status) {

  Get_core_resp_t    resp;
  resp.ret_val = this_cpu();
  systask_msgsend(status->src, &resp, sizeof(Get_core_resp_t));
}

/*
 * systask_do_gettime(Message_t*) -- man clock_gettime()
 */
void systask_do_gettime(Systask_msg_t *msg, Msg_status_t *status) {
  Gettime_req_t *req;
  Gettime_resp_t resp;
  uint64_t elapsed;
  struct tm tp;
  
  req = (Gettime_req_t *)msg;
  resp.type = SC_GETTIME;
  
  switch(req->clk_id) {
    
  case CLOCK_MONOTONIC:
    elapsed = ktimer_get_ms_elapsed();
    resp.tspec.tv_sec = elapsed / 1000;
    resp.tspec.tv_nsec = (elapsed % 1000) * 1000;
    resp.ret = 0;
    break;
    
  case CLOCK_REALTIME:
    /* Call ktime() */
    localtime(system_time, &tp);
    kmemcpy(&tp,&resp.tm_local, sizeof(struct tm));
    resp.ret = 0;
    break;
  default:
    resp.ret = -1; /* Fixme: EINVAL */
    break;
  }
  systask_msgsend(status->src, &resp, sizeof(Gettime_resp_t));	
}

void systask_do_pci_set_config(Systask_msg_t *msg, Msg_status_t *status) {
  /* Set the PCI configuration for a given device */
  Pci_set_config_req_t *req;
  Pci_set_config_resp_t resp;
  int ret_val;
  Pci_dev_t dev;
  
  req = (Pci_set_config_req_t *)msg;
  kmemcpy(&dev,&(req->pci_dev), sizeof(Pci_dev_t));
  
  pci_write_config(&dev, req->offset, req->val, req->width);
  ret_val = 0;
  
  resp.ret_val = 0;
  systask_msgsend(status->src, &resp, sizeof(Pci_set_config_resp_t));

  return;
}

/*TODO replace this with specialk */

uint64_t driver_mem_current = 0;
void systask_do_map_mmio(Systask_msg_t *msg, Msg_status_t *status){

  Map_mmio_req_t *req;
  Map_mmio_resp_t resp;

  req = (Map_mmio_req_t *)msg;

  /*TODO specialk will need to give us an address evenutally */
  kvmem_map_mmio(ksched_get_last(), 
		 DRIVER_MEM_START+driver_mem_current, 
		 &req->pci_dev.pci_dev_bars[req->bar]);

  resp.type = SC_MAP_MMIO;
  resp.tag = req->tag;
  resp.virt_addr = (uint64_t *)(DRIVER_MEM_START+driver_mem_current);

  driver_mem_current += req->pci_dev.pci_dev_bars[req->bar].bar_size;
  
  systask_msgsend(status->src, &resp, sizeof(Map_mmio_resp_t));

  return;
}

void systask_do_map_dma(Systask_msg_t *msg, Msg_status_t *status){

  Map_dma_req_t *req;
  Map_dma_resp_t resp;
  uint64_t phys_addr;
  req = (Map_dma_req_t *)msg;

  if(req->num_bytes % PAGE_SIZE)
    req->num_bytes += PAGE_SIZE - (req->num_bytes % PAGE_SIZE);

  phys_addr = kvmem_map_dma_region(ksched_get_last(), driver_mem_current+DRIVER_MEM_START, req->num_bytes); 

  resp.type = SC_MAP_DMA;
  resp.tag = req->tag;
  resp.virt_addr = (uint64_t *)(driver_mem_current+DRIVER_MEM_START);
  resp.phys_addr = (uint64_t *)phys_addr;

  driver_mem_current += req->num_bytes;

  systask_msgsend(status->src, &resp, sizeof(Map_dma_resp_t));

  return;
}

void systask_do_poll(Systask_msg_t *msg, Msg_status_t *status) {
  Poll_req_t *req;
  Poll_resp_t resp;
  Proc_t *p;
  
  p = pid_to_addr(status->src);
  req = (Poll_req_t *)msg;
  
  if( (req->timeout = -1) ) {
    /* Process did not specify a timeout
     * Sleep and wait indefinitely for some action
     */
    ktimer_new_alarm(7500, systask_unsleep,(void*)p);
  }
  else {
    /* Process specified a timeout period 
     * Schedule alarm accordingly 
     */
    ktimer_new_alarm(req->timeout, systask_unsleep,(void*)p);
    
  }
  
  /* Send a message back to the process informing it that I/O is available */
  resp.ret_val=0;
  systask_msgsend(status->src, &resp, sizeof(Poll_resp_t));

  return;
}

void systask_do_msi(Systask_msg_t *msg, Msg_status_t *status) {

   Msi_en_req_t  *msi = (Msi_en_req_t *)msg;
   Msi_en_resp_t  resp;

  uint32_t addr_reg;
  uint16_t data_reg;
  uint16_t ctrl_reg;

   resp.type = SC_MSI_EN;   

   Pci_dev_t* dev =  pci_fill_struct(msi->bus, msi->dev, msi->func );
   uint32_t cap_start_addr;
   
   if( pci_find_cap(dev, 0x05, &cap_start_addr) != 0 )
   {
     resp.ret = -1;
     systask_msgsend(status->src, &resp, sizeof(Msi_en_resp_t));
     return;
    }
    
    kprintf("cap offset is 0x%x \n", cap_start_addr);
    /*one of the intel manuals said you cannot clobber the reserved
     * fields and must preseve them, this is located 4 bytes up from the start
     */
    addr_reg = pci_read_config(dev, cap_start_addr + 4, 4);
    
    addr_reg |= (0xFEE << 20) | ((msi->apic_dst) <<  12);
    
    /*write the contents to the message address register */
    pci_write_config(dev, cap_start_addr + 4, addr_reg, 4);
    
    data_reg = (uint16_t)(pci_read_config(dev, cap_start_addr + 12, 2)); 
     
    data_reg |=  msi->vector | 
                 ((msi->delv_mode) << 7) |
                 ((msi->trigger_mode) << 15) | 
                 ((msi->trigger_level) << 14);
    
    /*write the contents to the message data register */
    pci_write_config(dev, cap_start_addr + 12, data_reg, 2);

    
    ctrl_reg = pci_read_config(dev, cap_start_addr + 2, 2);
#ifdef DEBUG
    kprintf("msi control register is 0x%x\n", ctrl_reg);
#endif    
    ctrl_reg |= 1;
    
    /*write the contents to the message address register */
    pci_write_config(dev, (cap_start_addr + 2), ctrl_reg, 2);
    
    
    ctrl_reg = pci_read_config(dev, cap_start_addr + 2, 2);
#ifdef DEBUG
    kprintf("msi control register after setting is 0x%x\n", ctrl_reg);
#endif    
    resp.ret = 0;
    systask_msgsend(status->src, &resp, sizeof(Msi_en_resp_t));
}

/* 
 * Systask_do_ps: lists all running processes and associated PIDs
 */
void systask_do_ps(Systask_msg_t *msg, Msg_status_t *status) {
  Ps_resp_t resp;    /* Response message    */
  void *rp;	     /* point to the response */

#ifdef KMALLOC_TRACKING
  kheapcheck(1,"ps-kheapcheck"); /* check the heap whenever ps is called */
  kprintf("\n");
#endif

#ifdef KVMEM_TRACKING
  kvmemcheck("ps-kvmemcheck");	/* print virtual memory allocations */
  kprintf("\n");
#endif 

  rp=&resp;
  kprintf("S  PID\tCMD\t\tParent\tChildren ; Zombies\n");
  resp.entries=0;
  ksched_ps(rp);		/* print the ready processes */
  kprintf("\n");
  resp.type = SC_PS;
  resp.ret  = 0;

  systask_msgsend(status->src, &resp, sizeof(Ps_resp_t));
}

/*
 * systask_do_sigaction: adjusts a process' preferred response to a signal
 */
void systask_do_sigaction(Systask_msg_t *msg, Msg_status_t *status) {

  Sigact_req_t  *req;
  Sigact_resp_t resp;
  
  int sig, ret_val;

  Proc_t *p;
  int signo;
  struct sigaction act;

  /* decode the message */
  req = (Sigact_req_t*) msg;

  signo = req->signo;
  act = req->act;

  /* find the process */
  p = pid_to_addr(status->src);

  /* save the address of the special sigreturn user function */
  p->sigret = req->sigret;

  /* build the response */
  resp.type = SC_SIGACT;
  resp.tag  = req->tag;
  resp.oldact = p->sigact[signo];     /* record old action */

  if ( act.sa_handler == SIG_PENDING ) {
    resp.oldact.sa_mask = p->sigpending;
    resp.ret_val = 0;
  }
  else if ( act.sa_handler ) { 
    resp.ret_val = _do_sigaction(p, signo, &act); 
  }
  /* else act is null, just return the old act */

  systask_msgsend(status->src, &resp, sizeof(Sigact_resp_t));

  /* check if anything needs to be dispatched */
  for ( sig = 1; sig <= _NSIG; sig++ ) {
    if ( sigismember(&(p->sigdispatch), sig) ) {

      sigdelset(&(p->sigdispatch), sig);

      ret_val = _do_kill(p, sig);

      /* we might want to keep checking, or we want to run the process
	 right now. It depends what we do in response to the signal. */
      if ( ret_val != SIG_IGNORED && ret_val != SIG_BLOCKED &&
	   ret_val != SIG_PROC_UNBLOCKED )
	return;
    }
  }

  return;
}

void systask_do_sigreturn(Systask_msg_t *msg, Msg_status_t *status) {

  Proc_t *p;
  ucontext_t *last_ucp;
  int sig, ret_val;

  /* get process */
  p = pid_to_addr(status->src);
  last_ucp = p->ucp;

  /* restore the context */
  p->mc      = p->ucp->uc_mcontext;
  p->sigmask = p->ucp->uc_sigmask;
  p->ucp     = p->ucp->uc_link;

  /* check if anything needs to be dispatched */
  for ( sig = 1; sig <= _NSIG; sig++ ) {
    if ( sigismember(&(p->sigdispatch), sig) ) {

      sigdelset(&(p->sigdispatch), sig);

      ret_val = _do_kill(p, sig);
 
      /* make the process resume! */
      if ( ret_val == SIG_HANDLED ) {
	/*  TODO restore_proc_vmem was here */
	restore_user_proc(p);
      }
      else if ( ret_val == SIG_PROC_DESTROYED )
	return;
    }
  }  

  /* make the process resume! */
  /*  TODO restore_proc_vmem was here */
  restore_user_proc(p);

  /* no return message. */
  return;
}

/******************************************************************************
 ******************************* HELPER FUNCTIONS  ****************************
 *****************************************************************************/

/*
 * helper function to do the acutal processing of the signal.
 * returns 1 for error and 0 for success
 */
static int _do_kill(Proc_t *p, int signo) {

  ucontext_t uc;

  void *sp_kaddr;

  /* TODO: check for "ANY" or "ALL" special pid
   *       if so, change goto sendmsg to continue */

  /* check to make sure process being signalled exists */
  if ( !p ) {
    return -1;
  }

  /* process exists. process signal */

  if ( sigismember(&(p->sigignore), signo) ) {
    /* ignore signal */
    return SIG_IGNORED;
  }
  else if ( sigismember(&(p->sigmask), signo) ) {
    /* block the signal */
    sigaddset(&(p->sigpending), signo);
    return SIG_BLOCKED;
  }
  else if ( sigismember(&(p->sigcatch), signo) ) {
    /* execute the process' handler */

    /* handling for sigsuspend */
    if ( p->waiting_for == WAIT_ON_SIGNAL ) {
      ksched_unblock(p);
      p->sigmask = p->sigmask2;
      p->waiting_for = 0;
    }

    /* TODO: FIND BOTTOM OF USER STACK */
    /* make sure that there is sufficient space on the stack */
#if 0
    if ( p->mc.rsp - (sizeof(ucontext_t) + sizeof(uint64_t))
         <  ) {
      return -1;
    }
#endif

    /* get kernel logical address of userspace stack pointer */
    sp_kaddr = (void*)p->mc.rsp;

    /* find addr to store sigcontext */
    sp_kaddr = sp_kaddr - sizeof(ucontext_t);

    /* initialize sigcontext */
    uc.uc_mcontext  = p->mc;      /* save the current state of the machine */
    uc.uc_link      = p->ucp;     /* point to the current saved state      */
    uc.uc_sigmask   = p->sigmask; /* save sigmask of current state         */
    p->sigmask = p->sigact[signo].sa_mask; /* sigs to block now       */
    sigaddset(&(p->sigmask), signo);      /* default to block signo  */

    /* save location of this context on user stack */
    p->ucp = (ucontext_t*)sp_kaddr;

    /* push sigcontext onto user stack */
    kmemcpy(sp_kaddr, &uc, sizeof(ucontext_t));
    p->mc.rsp = (uint64_t)p->mc.rsp - sizeof(ucontext_t);

    /* push return address */
    sp_kaddr = sp_kaddr - sizeof(uint64_t);
    p->mc.rsp = (uint64_t)p->mc.rsp - sizeof(uint64_t);
    *(uint64_t*)(sp_kaddr) = (uint64_t)p->sigret;

    /* pass argument in rdi per calling conventions */
    p->mc.rdi = signo;

    /* set instruction pointer to handler */
    p->mc.rip = (reg_t)p->sigact[signo].sa_handler;

    return SIG_HANDLED;
  }

  /* handle the signal using defaults */
  switch ( signo ) {

  /* terminate process */
  case SIGALRM:     /* alarm clock                         */
  case SIGHUP:      /* hangup                              */
  case SIGINT:      /* terminal Interrupt Signal (ctrl+c)  */
  case SIGKILL:     /* kill (cannot be caught or ignored)  */
  case SIGPIPE:     /* write on a pipe with noone to read  */
  case SIGTERM:     /* termination signal                  */
  case SIGUSR1:     /* user defined signal 1               */
  case SIGUSR2:     /* user defined signal 2               */
  case SIGPOLL:     /* pollable event                      */
  case SIGPROF:     /* profiling timer expired             */
  case SIGVTALRM:   /* virtual timer expired               */
    update_proc_status(p,signo,SIG_EXITED);
    _do_kill(p->parent, SIGCHLD);
    destroy_proc(p, signo);
    return SIG_PROC_DESTROYED;

  /* stop the process */
  case SIGSTOP:     /* stop executing (no catch or ignore) */
  case SIGTSTP:     /* terminal stop signal                */
  case SIGTTIN:     /* background process attempting read  */
  case SIGTTOU:     /* background process attempting write */

    if ( p->status & STOPPED ) {
      update_proc_status(p, signo, SIG_STOPPED | STOPPED);
    }
    else {
      ksched_block(p);
      update_proc_status(p, signo, SIG_STOPPED);
    }

    _do_kill(p->parent, SIGCHLD);

    return SIG_PROC_BLOCKED;

  /* continue the process */
  case SIGCONT:     /* continue if stopped else ignore     */
    if ( (p->status & (SIG_STOPPED | STOPPED)) == SIG_STOPPED ) {
      if ( p->waiting_for != WAIT_ON_SIGNAL && !WIFCONTINUED(p->status)) {
	ksched_unblock(p);
      }
    }
    else if ( (p->status & STOPPED) == STOPPED )  {
      update_proc_status(p,0,STOPPED);
      update_proc_status(p,0,0); /* mark as reported */
    }
    
    _do_kill(p->parent, SIGCHLD);

    return SIG_PROC_UNBLOCKED;

  /* ignore the signal */
  case SIGCHLD:     /* child process terminated, stopped   */
  case SIGURG:      /* data available at socket            */
    return SIG_IGNORED;

  default:
    return -1;
  }
  
  /* never reached */
  return -1;
}

/*
 * helper function that does the heavy lifting for sys_do_sigaction
 * returns 1 on failure, 0 on success
 */
int _do_sigaction(Proc_t *p, int signo, struct sigaction *act) {

  /* unblockable and unhandelable signals */
  if ( signo == SIGKILL || signo == SIGSTOP )
    return 0;

  /* make sure that it is a valid signal number */
  if ( (signo < 1 || signo > _NSIG) && signo != SIGPMASK && signo != SIGSUSP )
    return 1;

  /* call came from sigprocmask */
  if ( signo == SIGPMASK ) {

    /* block */
    if ( act->sa_handler == (__sighandler_t)SIG_BLOCK ) {
      
      p->sigmask |= act->sa_mask;

    }   /* end if block */

    /* set mask */
    else if ( act->sa_handler == (__sighandler_t)SIG_SETMASK ) {

      p->sigdispatch |= (p->sigpending & ~(act->sa_mask));
      p->sigpending  &= ~(p->sigdispatch);
      p->sigmask = act->sa_mask;

    }
    /* unblock */
    else if ( act->sa_handler == (__sighandler_t)SIG_UNBLOCK ) {
      
      p->sigdispatch |= (p->sigpending & act->sa_mask);
      p->sigpending  &= ~(p->sigdispatch);
      p->sigmask &= ~(act->sa_mask);

    }     /* end if unblock */
  }       /* end if call came from sigprocmask */

  /* call came from a sigsuspend */
  else if ( signo == SIGSUSP ) {
    
    /* save the old sigmask */
    p->sigmask2 = p->sigmask;
    
    /* set new sigmask */
    p->sigmask = act->sa_mask;

    /* waiting on a signal */
    p->waiting_for = WAIT_ON_SIGNAL;

    /* suspend process */
    ksched_block(p);
  }

  /* call came from a plain sigaction */
  else { 

    /* if its an ignore */
    if ( act->sa_handler == SIG_IGN ) {
      sigaddset(&(p->sigignore),  signo);
      sigdelset(&(p->sigpending), signo);
      sigdelset(&(p->sigcatch),   signo);
      sigdelset(&(p->sigmask),    signo);
    }
    else {
      sigdelset(&(p->sigignore),  signo);

      /* if we want to restore defaults */
      if ( act->sa_handler == SIG_DFL )
	sigdelset(&(p->sigcatch), signo);
      else /* we want to catch it */
        sigaddset(&(p->sigcatch), signo);
    }

    /* save new action */
    p->sigact[signo] = *act;

  } 

  return 0;
}

/*
 * systask_unsleep(Proc_t*, Message_t*)
 *
 *  This function is a bit of a dirty trick to make fork work.  It is called
 *  by the PIT interrupt handler after a process's sleep timer expires, and it
 *  essentially sends a message to the process to wake it up.  The message
 *  is made to look like it's from the system task.
 *
 */
static void systask_unsleep(void *pproc) {
  Message_t msg;
  Usleep_resp_t resp;
  Proc_t *dst_p;
  
  dst_p = (Proc_t *)pproc;
  
  /* If message-passing ever changes, this will break. */
  msg.src = SYS;
  msg.dst = dst_p->pid;
  msg.len = sizeof(Usleep_resp_t);
  msg.status = NULL;
  msg.buf = &resp;
  resp.type = SC_USLEEP;
  
  kmsg_send(&msg);
}

/*
 * systask_unwait(Proc_t*, Message_t*)
 *
 *  This function is a bit of a dirty trick to make wait work.  It is called
 *  by the kwait module when it is time for a waiting process to wake up.
 *  Essentially this function sends a message to the process to wake it up.
 *  The message is made to look like it's from the system task.
 */
static void systask_unwait(Proc_t *pproc, int wpid, int status, unsigned int tag) {
  Message_t msg;
  Waitpid_resp_t resp;
  Proc_t *dst_p;
  
  dst_p = pproc;

  /* If message-passing ever changes, this will break. */
  msg.src = SYS;
  msg.dst = dst_p->pid;
  msg.len = sizeof(Waitpid_resp_t);
  msg.status = NULL;
  msg.buf = &resp;
  resp.type = SC_WAITPID;
  resp.wpid = wpid;
  resp.exit_val = status;
  resp.tag = tag;
  
  kmsg_send(&msg);
}




/** Kernel routine for the alarm() syscall
 * This will create a new timer and check if it is expired	
 * If the latter one is true, it will signal the process	
 @Author Martin Osterloh
 @Date July 23rd 2014
*/
static void alarm_cb(void* arg) {
  _do_kill((Proc_t*)arg, SIGALRM);
}

void systask_do_alarm(Systask_msg_t *msg, Msg_status_t *status) {	
  Proc_t *p;
  p=pid_to_addr(status->src);

  ktimer_new_alarm(
		   (((Alarm_req_t*)msg)->seconds)*1000,
		   alarm_cb,
		   (void*)p);
}

void systask_do_getstdio(Systask_msg_t *msg, Msg_status_t *status) {
  getstdio_req_t  *req;
  getstdio_resp_t resp;
  Proc_t *p;
  int fd;

  req = (getstdio_req_t *)msg;
  p = pid_to_addr(status->src);
  fd = ((getstdio_req_t*)msg)->fd;
  if(fd>=0 && fd<=2)           /* stdio */
    resp.pid=p->stdio[fd];

  resp.type = SC_GETSTDIO;
  resp.tag = req->tag;
  systask_msgsend(status->src, &resp, sizeof(getstdio_resp_t));
}

void systask_do_redirect(Systask_msg_t *msg, Msg_status_t *status) {
  redirect_resp_t    resp;
  Proc_t *p;
  p = pid_to_addr(status->src);
  p->stdio[0]=((redirect_req_t*)msg)->inpid;
  p->stdio[1]=((redirect_req_t*)msg)->outpid;
  p->stdio[2]=((redirect_req_t*)msg)->errpid;
  /*
  kprintf("[SYS] redirect(%d,%d,%d) for pid=%d\n",
	  p->stdio[0],p->stdio[1],p->stdio[2],status->src);
  */
  systask_msgsend(status->src, &resp, sizeof(redirect_resp_t));
}

#ifdef KERNEL_DEBUG
/*
 * systask_do_kprintint(Message_t*) -- just print an integer
 */
void systask_do_kprintint(Systask_msg_t *msg, Msg_status_t *status) {
  kprintint_resp_t    resp;
  kprintf("[SYS] %s: %d, from %d\n",
	  ((kprintint_req_t*)msg)->str,
	  ((kprintint_req_t*)msg)->val,
	  ((kprintint_req_t*)msg)->src);
  systask_msgsend(status->src, &resp, sizeof(kprintint_resp_t));
}

/*
 * systask_do_kprintstr(Message_t*) -- just print an streger
 */
void systask_do_kprintstr(Systask_msg_t *msg, Msg_status_t *status) {

  kprintstr_resp_t    resp;
  kprintf("[SYS] kprintstr:%s\n",((kprintstr_req_t*)msg)->str);
  systask_msgsend(status->src, &resp, sizeof(kprintstr_resp_t));
}

#endif
