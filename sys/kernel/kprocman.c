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
 * Filename: procman.c
 *
 * Description: Contains all the code for managing processes and scheduling.
 *              This includes swapping processes, creating new processes,
 *              destroying processes, suspending processes, etc.
 *
 *****************************************************************************/

#include <constants.h>            /* For  eflags constants  */
#include <memory.h>
#include <elf.h>                  /* For ELF structures     */
#include <kernel.h>               /* For segment constants  */
#include <kstdio.h>               /* For kprintf             */
#include <kstring.h>
#include <file_abstraction.h>     /* hack for now           */
#include <kvmem.h>                /* For paging protections */
#include <kmalloc.h>              /* For malloc             */
#include <kqueue.h>                /* For qput qget qopen    */
#include <elf_loader.h>           /* For elf_load_file()    */
#include <kmsg.h>                 /* For kmsg_purge()       */
#include <asm_subroutines.h>
#include <interrupts.h>
#include <procman.h>
#include <random.h>
#include <kwait.h>
#include <ksched.h>
#include <pes.h>
#include <signal.h>
#include <sbin/vgad.h>
#include <sbin/kbd.h>
#include <khash.h>

#ifdef KPLT
#include <diversity.h>
#endif

uint64_t* temp_child_heap_ptr;

/******************************************************************************
 ***************************** PRIVATE DECLARATIONS ***************************
 *****************************************************************************/

/* A simple table structure for looking up a Proc_t given a pid.  
 * The key is the pid. I assume that the PIDs are evenly distributed,
 * and the "hash" function is simply (pid & PT_MASK).  Should lead to
 * pretty fast search times. */
#define PLUT_SLOTS 64
#define HASH_FUNC(n) ((n) % PLUT_SLOTS)
static hashtable_t *proc_htable;
static void lut_init();                 /* init the lut                     */

static void new_cr3_target(Proc_t *p, int clone);

/* the first new_proc should use the currently loaded cr3 target since there 
   is already a kernel there. This variable will help us decide when we need 
   to start making new tables in new_proc */
static int first = 1;

/******************************************************************************
 *************************** FUNCTION DEFINITIONS *****************************
 *****************************************************************************/

/******************************************************************************
 *
 * Function: get_proc_lut()
 *
 * Description: returns the proc_lut memory location
 *              fixes ps
 *
 *****************************************************************************/
hashtable_t *get_proc_lut(){
  return proc_htable;
}


/******************************************************************************
 *
 * Function: sched_init()
 *
 * Description: Initialize all data structures, counters, etc. associated with
 *              the scheduler.
 *
 *****************************************************************************/
void procman_init() {

  /* Init the pid-to-proc_t lookup table */
  lut_init();

#ifdef DEBUG
  kprintf("[Procman] Initialized the Process Look Up Table\n");
#endif

  /* Tracks the next user PID to be used... better not roll over... */
  /*
   * This is at a static address to facilitate forensics (Steve K's work). 
   * Disregarding forensics, this can be made a regular static variable like
   * the sys_pid_counter variable.
   */
  user_pid_counter = &pid_counter;
  *user_pid_counter = USER_PID_START;

  /* Initialized the number of issued user pages to zero */
  number_of_issued_pages = 0;

#ifdef DEBUG
  kprintf("	[Procman] Process array addr %x\n", proc_htable);
#endif

  return;
}

static void copy_mapping_q(void *childq, void *elementp) {

  struct memory_region *mr = (struct memory_region*)kmalloc_track(PROCMAN_SITE, sizeof(struct memory_region));
  kmemcpy(mr, elementp, sizeof(struct memory_region));

  qput(childq, mr);

  if ( mr->type == HEAP_REGION )
    temp_child_heap_ptr = (uint64_t*)mr;

  return;
}

Proc_t *new_kernel_proc(uint64_t code_pointer, pid_t pid) {

  Proc_t *p;

  p = kmalloc_track(PROCMAN_SITE,sizeof(Proc_t)); /* Allocate Proc Structure */
  kmemset(p,0,sizeof(Proc_t));

  p->pl = PL_0;

  /* kernel machine context*/
  p->kmc.cs = (reg_t)K_CODESEG;
  p->kmc.ss = (reg_t)K_DATASEG;

  p->kmc.eflags = (reg_t)(EFLAGS_BASE | EFLAGS_IF);   
    
  p->kmc.rsp = (reg_t)system_stack_base;
  p->kmc.rbp = (reg_t)system_stack_base;

  /* need to assign rip */
  p->kmc.rip = (reg_t)code_pointer;

  p->pid = pid;                                         /* Use supplied pid */

  /* when ksched yielding we transfer laterally from one proc kernel to another
     procs kernel. Regardless if you are a clone or a new proc a save location
     for a procs floating point state needs to be maintained */
  //  p->kmc.sse = pes_new_save(&(p->pes_hack_kernel));

  new_cr3_target(p, 0);

  return p;
}

/******************************************************************************
 *
 * Function: new_proc
 *
 * Description: Initializes a new proc_t structure.
 *
 *****************************************************************************/

Proc_t *new_proc(uint64_t code_pointer, int pl, pid_t pid, Proc_t *parent) {

  Proc_t *p;
  int i, clone;

  /* Allocate Process Structure */
  p = kmalloc_track(PROCMAN_SITE,sizeof(Proc_t));
  kmemset(p,0,sizeof(Proc_t));

  p->argc = 0;
  p->argv = NULL;
  p->envc = 0;
  p->env = 0x00000;

  p->pes_hack_user = 0x0;
  p->pes_hack_kernel = 0x0;

  /* TODO: code_pointer is stupid and not used. Replace it? */
  clone = code_pointer ? 0 : 1;
  
  /* if the new proc is NOT a clone */
  if ( !clone ) {

    /* set default stdio processes */
    p->stdio[0] = KBD;
    p->stdio[1] = VGAD;
    p->stdio[2] = VGAD;

    /*** MACHINE CONTEXT ***/
    p->mc.cs = (reg_t)(pl == PL_0 ? K_CODESEG : U_CODESEG);
    p->mc.ss = (reg_t)(pl == PL_0 ? K_DATASEG : U_DATASEG);
    p->mc.eflags = (reg_t)(EFLAGS_BASE | EFLAGS_IF);           /* RFLAGS */

    /*** PRIVILEGE INFO ***/
    p->pl = pl;
    p->mc.eflags |= eflags_iopl(pl);

    /* kernel machine context*/
    p->kmc.cs = (reg_t)K_CODESEG;
    p->kmc.ss = (reg_t)K_DATASEG;

    p->kmc.eflags = (reg_t)(EFLAGS_BASE | EFLAGS_IF);   

    p->kmc.rsp = (reg_t)system_stack_base;
    p->kmc.rbp = (reg_t)system_stack_base;

    p->allocated_pages = 0; /* no user pages allocated for this process yet */
    
    /* signal member initializations */
    sigemptyset(&(p->sigignore));
    sigemptyset(&(p->sigcatch));
    sigemptyset(&(p->sigmask));
    p->sigret = NULL;
    p->ucp = NULL; 
    kmemset(p->sigact, 0, _NSIG * sizeof(struct sigaction));
  }
  else { /* for a clone, copy parent structures */

    kmemcpy(p, parent, sizeof(Proc_t));

    p->argv = kmalloc_track(PROCMAN_SITE, parent->argc*sizeof(char*));
    for ( i = 0; i < parent->argc; i++ ) {
      p->argv[i] = kmalloc_track(PROCMAN_SITE, kstrlen(parent->argv[i])*sizeof(char));
      kstrncpy(p->argv[i], parent->argv[i], kstrlen(parent->argv[i]));
    }

    p->env = (uint64_t)kmalloc_track(PROCMAN_SITE, PAGE_SIZE);
    kmemset((void*)p->env, 0, PAGE_SIZE);

    uint64_t ptr = p->env + sizeof(char *)*(p->envc + 1);

    for ( i = 0; i < p->envc; i++ ) {
      ((char**)p->env)[i] = (char*)ptr;
      kstrncpy((char*)ptr, ((char**)parent->env)[i], kstrlen(((char**)parent->env)[i]) + 1);
      ptr += kstrlen(((char**)parent->env)[i]) + 1;
    }
    
  } 

  /* now we have stuff we have to do for the proc no matter what */

  /* need to assign rips to proc depending on clone or not */
  p->mc.rip = (reg_t)(clone ? parent->mc.rip : code_pointer /* to be overwritten in load_elf_proc */);
  p->kmc.rip = (reg_t)(clone ? (void*)kernel_exit : (void*)kstart);

  /* initialize the memory region queue */
  p->mapped_memory_regions = qopen();
  p->heap_region = 0x0;

  /* Pick a PID */
  if (pid == PROC_NONE) {
    p->pid = *user_pid_counter;                      /* allocate user pid */
    *user_pid_counter += 1;
  }
  else
    p->pid = pid;                         /* Use supplied pid */
  
  lut_add(p);                                     /* Add to pid-to-proc LUT */

  if ( p->pid != IDLE_PROC )
    kwait_new(p->pid, parent ? parent->pid : 0);

  p->mc.sse = pes_new_save(&(p->pes_hack_user));  /* FPU/MMX/SSE save area */

  /* posix says that even clones get fresh copies of these */
  sigemptyset(&(p->sigpending));
  sigemptyset(&(p->sigdispatch));

  new_cr3_target(p, clone);

  update_proc_status(p,0,CONTINUED);
  update_proc_status(p,0,0);

  return p;           /* Return the process struct. */
}

typedef struct addr_pair {
  uint64_t curr;
  uint64_t vaddr;
  uint64_t paddr;
} addr_pair;

#ifdef KPLT
static int find_kplt_fn(void *elementp, const void *keyp) {
  return ((addr_pair*)elementp)->vaddr == ((*(uint64_t*)keyp) & ~(uint64_t)0xfff);
}
#endif 

static void new_cr3_target(Proc_t *p, int clone) {

  int i;
  Proc_t *parent;

  queue_t *addr_q;
  addr_pair *addrs;

  uint64_t special_k, special_k_base;
  uint64_t pml4t_phys, pml4t_virt;
  uint64_t pdpt_phys, pdpt_virt;
  uint64_t pd_phys, pd_virt;
  uint64_t pt_phys, pt_virt;
  uint64_t frame_phys, frame_virt;

  int pml4t_idx, pdpt_idx, pd_idx, pt_idx;

#ifdef KPLT
  queue_t *todo_q;
  addr_pair *todo;
  uint8_t *kpltp;
  uint64_t new_vkplt;
  uint64_t fn_addr;
  uint64_t rand_fn_addr;
  uint32_t fn_size;
  uint32_t fn_align;
  uint64_t kplt_size;
  uint64_t remainder;
  unsigned char jmp[20];  
  uint64_t searchval, ptsearchval;
  partition_t *parts, *part;
#endif
  
  parent = p->parent;
  
  /*** PAGING AND MEMORY ***/                            /* PML4T */
  if ( !first ) {

#define TEMP_MAP(p, v, v2)						\
    vmem_alloc((uint64_t*)special_k, PAGE_SIZE, PG_RW);			\
    v = special_k;							\
    p = virt2phys((void*)v);					        \
    addrs = (addr_pair*)kmalloc_track(PROCMAN_SITE, sizeof(addr_pair));	\
    addrs->curr = special_k;						\
    addrs->paddr = (uint64_t)p;						\
    addrs->vaddr = (uint64_t)v2;					\
    qput(addr_q, addrs);						\
    special_k += PAGE_SIZE; 

    /* find an unused PML4TE to enable the temporary virtual mapping to the new
       CR3 Target */
    for ( i = 0; i < 512; i++ )
      if ( !((union pt_entry*)PML4TE2vaddr(i))->present )
        break;

    /* check if we found a free one */
    if ( i == 512 ) {
      kprintf("NOT ENOUGH SPACE IN THE PML4T!!\n");
      panic();
    }

    /* just assume that a blank PML4TE is enough virtual space for the addrs 
       we will need. TODO: REplace with special k */
    special_k_base = (uint64_t)idx2vaddr(i, 0, 0, 0);
    special_k = special_k_base;

    /* we will be vmem allocing paging structures and stuff for the guest. 
       later we will need to fix up the frame array so that the frames do not 
       have the record of the vaddr that we alloced here but rather the one 
       by which the new proc will be able to access it */
    addr_q = qopen();

    /* map the CR3 target into our address space */
    TEMP_MAP(pml4t_phys, pml4t_virt, PML4TE2vaddr(0));
    kmemset((void*)pml4t_virt, 0, PAGE_SIZE);
    p->cr3_target = (struct page_map_level_4_table*)pml4t_phys;

    for ( pml4t_idx = 0; pml4t_idx < 512; pml4t_idx++ ) {

      if ( !((union pt_entry*)PML4TE2vaddr(pml4t_idx))->present ||
	   pml4t_idx == virt2pml4t(special_k) || 
	   pml4t_idx == PML4T_RECURSIVE_ENTRY_IDX) {
	*(uint64_t*)(pml4t_virt + (sizeof(union pt_entry)*pml4t_idx)) = 0x0;
	continue;
      }
      
      pdpt_phys = pdpt_virt = 0;

      for ( pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++ ) {

	if ( !((union pt_entry*)PDPTE2vaddr(pml4t_idx, pdpt_idx))->present ) 
	  continue;

	pd_phys = pd_virt = 0;
	
	for ( pd_idx = 0; pd_idx < 512; pd_idx++ ) {
	  if ( !((union pt_entry*)PDE2vaddr(pml4t_idx, pdpt_idx, pd_idx))->present )
	    continue;
	  
	  pt_phys = pt_virt = 0;
	  
	  for ( pt_idx = 0; pt_idx < 512; pt_idx++ ) {
	    
	    if ( !((union pt_entry*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->present )
	      continue;
	    
	    /* generate page of memory for new proc */
	    if ( !((union page*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->global ) {

	      if ( !((union pt_entry*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->us ) {
		/* page is not global and it _is_ kernel space, set up fresh memory for it. */
		TEMP_MAP(frame_phys, frame_virt, idx2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx));
		kmemset((void*)frame_virt, 0, PAGE_SIZE);
	      }
	      else if ( clone == 1 ) {
		/* it is user space and we are making a clone, make new memory and copy contents over */
		TEMP_MAP(frame_phys, frame_virt, idx2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx));
		kmemcpy((void*)frame_virt, idx2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx), PAGE_SIZE);
	      }
	      else /* userspace memory but we are not cloning */
		continue;
	    }
	    
	    /* create necessary paging structs */
	    if ( !pt_phys ) {
	      if ( !pd_phys ) {
		if ( !pdpt_phys ) {
		  TEMP_MAP(pdpt_phys, pdpt_virt, PDPTE2vaddr(pml4t_idx, 0));
		  kmemset((void*)pdpt_virt, 0, PAGE_SIZE);
		}
		TEMP_MAP(pd_phys, pd_virt, PDE2vaddr(pml4t_idx, pdpt_idx, 0));
		kmemset((void*)pd_virt, 0, PAGE_SIZE);
	      }
	      TEMP_MAP(pt_phys, pt_virt, PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0));
	      kmemset((void*)pt_virt, 0, PAGE_SIZE);
	    }
	    
	    /* place newly generated memory page into paging structs */
	    if ( ((union page*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->global )
	      /* if it is global, copy the mapping to one common global page. */
	      *(uint64_t*)(pt_virt + (sizeof(union pt_entry)*pt_idx)) = 
		*(uint64_t*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx);  
	    else {
	      if ( !((union pt_entry*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, pt_idx))->us ) 
		setup_table((void*)frame_phys, (void*)(pt_virt + (sizeof(union pt_entry)*pt_idx)), PG_RW);//get_flags((union page*)(pt_virt + (sizeof(union pt_entry)*pt_idx))));
	      else 
		setup_table((void*)frame_phys, (void*)(pt_virt + (sizeof(union pt_entry)*pt_idx)), PG_RW | PG_USER);//get_flags((union page*)(pt_virt + (sizeof(union pt_entry)*pt_idx))));
	    }
	  }
	  if ( pt_phys )
	    setup_table((void*)pt_phys,(void*)(pd_virt + (sizeof(union pt_entry)*pd_idx)), PG_RW | PG_USER);
	  else
	    *(uint64_t*)(pd_virt + (sizeof(union pt_entry)*pd_idx)) = 0x0;
	}
	if ( pd_phys )
	  setup_table((void*)pd_phys,(void*)(pdpt_virt + (sizeof(union pt_entry)*pdpt_idx)), PG_RW | PG_USER );
	else 
	  *(uint64_t*)(pdpt_virt + (sizeof(union pt_entry)*pdpt_idx)) = 0x0;
      }
      
      if ( pdpt_phys ) 
	setup_table((void*)pdpt_phys, (void*)(pml4t_virt + (sizeof(union pt_entry)*pml4t_idx)), PG_RW | PG_USER);
      else 
	*(uint64_t*)(pml4t_virt + (sizeof(union pt_entry)*pml4t_idx)) = 0x0;
    }

    /* set up recursive pointer */
    setup_table((void*)p->cr3_target, (void*)(pml4t_virt+(sizeof(union pt_entry)*PML4T_RECURSIVE_ENTRY_IDX)), PG_RW | PG_NX);

    if ( clone == 1 ) {
      /* make a copy of the mapping q */
      qapply2(parent->mapped_memory_regions, p->mapped_memory_regions, copy_mapping_q);
      /** Quick and dirty way to get the pointer out of the qapply
	  copy_mapping_q function*/
      p->heap_region = (struct memory_region*)temp_child_heap_ptr;
    }

#ifdef KPLT

    /* 3 part process for handling kplt. Recalculate, rebuild, remap. See paper. */

    /* queue to use later during the remapping phase */
    todo_q = qopen();
    
    /* RECALCULATE -> first, gather the correct info about the current state of the system. */
    parts = init_partitions();
    for ( kpltp = (uint8_t*)kplt; *kpltp; kpltp += 20 ) {
 
      /* find the address of the function that this kplt points to. */
      fn_addr = 0x0;
      for ( i = 2; i < 10; i++ )
	fn_addr |= (uint64_t)(*(uint8_t*)(kpltp + i)) << (i - 2)*8;
      fn_size = *(uint32_t*)(kpltp + 12);
      fn_align = *(uint32_t*)(kpltp + 16);

      /* add the the available pool of memory to pick new addresses */
      add_to_partitions(parts, fn_addr, fn_size);
    }

    kpltp = (uint8_t*)kplt;
    while ( *kpltp ) {

      /* RECALCULATION CONTINUES */

      /* find the address of the function that this kplt points to. */
      fn_addr = 0x0;
      for ( i = 2; i < 10; i++ )
	fn_addr |= (uint64_t)(*(uint8_t*)(kpltp + i)) << (i - 2)*8;
      fn_size = *(uint32_t*)(kpltp + 12);
      fn_align = *(uint32_t*)(kpltp + 16);

      /* now, find a new function addr */
      rand_fn_addr = pick_random_place(parts, fn_size, PAGE_SIZE, &part);
      rand_fn_addr |= (fn_addr & 0xfff); /* make sure the last bits match on all addrs for a given function. */
      split_partition(part, rand_fn_addr, rand_fn_addr + fn_size);

      /* REBUILD */

      /* build the new kplt entry */
      jmp[0] = 0x48;
      jmp[1] = 0xb8;
      for ( i = 2; i < 10; i++ )
	jmp[i] = ((char)(rand_fn_addr >> (i-2)*8) & 0xff);
      jmp[10] = 0xff;
      jmp[11] = 0xe0;
      *(uint32_t*)(&jmp[12]) = fn_size;
      *(uint32_t*)(&jmp[16]) = fn_align;

      /* find the current proc's temp virtual mapping to the new proc's kplt */
      new_vkplt = ((addr_pair*)qsearch(addr_q, find_kplt_fn, &kpltp))->curr;

      /* copy the whole entry, or up to where it hits the a boundary. */
      kmemcpy( (void*)(new_vkplt + ((uint64_t)kpltp % PAGE_SIZE)), jmp, 
	       PAGE_SIZE - ((uint64_t)kpltp % PAGE_SIZE) < 20 ? PAGE_SIZE - ((uint64_t)kpltp % PAGE_SIZE) : 20);

      /* need to handle the case where the kplt entry crosses a page boundary */
      if ( ((uint64_t)kpltp & ~(uint64_t)0xfff) != (((uint64_t)kpltp+20) & ~(uint64_t)0xfff) ) {
	remainder = 20 - ( PAGE_SIZE - ((uint64_t)kpltp % PAGE_SIZE) );
      	kpltp += PAGE_SIZE - ((uint64_t)kpltp % PAGE_SIZE);
	new_vkplt = ((addr_pair*)qsearch(addr_q, find_kplt_fn, &kpltp))->curr;
	kmemcpy( (void*)new_vkplt, jmp + 20 - remainder, remainder);
	kpltp += remainder;
      }
      else 
	kpltp += 20;
      
      /* REMAP PART 1: preparation */

      /* now, do the bookkeeping to later adjust the mapping to what the new kplt says */
      searchval = fn_addr & ~(uint64_t)0xfff;
      do {
	/* find the pt that addresses the parent's fn_addr for the child */
	ptsearchval = (uint64_t)PTE2vaddr(virt2pml4t(searchval), virt2pdpt(searchval), virt2pd(searchval), 0);
	addrs = (addr_pair*)qsearch(addr_q, find_kplt_fn, &ptsearchval);
	if ( !addrs ) {
	  kprintf("null ptvirt in kplt stuff\n");
	  panic();
	}
	else 
	  pt_virt = addrs->curr;

	/* set the entry to 0 */
	/* important note: we really should be doing this for all of the 
	   paging structures up the chain like a full-blown vmem_free does 
	   (see vmem_layer.c). Doing what we are doing might leave some 
	   structures that arnt being used still mapped. However, the virtual 
	   memory system is good enough that it will use these rather than 
	   allocating new ones if it can, and _should_ be smart enough to 
	   clean them up on process deletion, so no memory leak overall. */
	*(uint64_t*)(pt_virt + virt2pt(searchval)*sizeof(union pt_entry)) = 0x0;

	/* find or build record of the actual page with the functionality */
	addrs = (addr_pair*)qsearch(addr_q, find_kplt_fn, &searchval);
	if ( !addrs ) {
	  addrs = kmalloc_track(PROCMAN_SITE, sizeof(addr_pair));
	  addrs->curr = searchval;
	  addrs->paddr = virt2phys((void*)searchval);
	}

	/* modify the addrs struct to match the new kplt, and add it to a todo list. */
	addrs->vaddr = rand_fn_addr & ~(uint64_t)0xfff;
	qput(todo_q, addrs);

	/* advance the search */
	searchval += PAGE_SIZE;
	rand_fn_addr += PAGE_SIZE;

	/* do-while loop to handle functions that cross page boundaries */
      } while ( searchval < fn_addr + fn_size );
    }

    /* REMAP PART 2: execution */
    
    /* now, set up paging structures at the new and randomly chosen vaddrs for each 
       kplt function taken from the todo queue. */
    /* note, could be many todos per function. Doesnt matter */
    while ( (todo = qget(todo_q)) ) {

      /* where in the child's paging structures with this new mapping go? */
      pml4t_idx = virt2pml4t(todo->vaddr);
      pdpt_idx = virt2pdpt(todo->vaddr);
      pd_idx = virt2pd(todo->vaddr);
      pt_idx = virt2pt(todo->vaddr);

      /* find the pml4t */
      searchval = (uint64_t)PML4TE2vaddr(0);
      pml4t_virt = ((addr_pair*)qsearch(addr_q, find_kplt_fn, &searchval))->curr;
      if ( !pml4t_virt ) {
	kprintf("MAJOR PROBLEM IM PROCESS CREATION WITH KPLT FIXING\n");
	panic();
      }

      /* find or build the pdpt */
      searchval = (uint64_t)PDPTE2vaddr(pml4t_idx, 0);
      addrs = (addr_pair*)qsearch(addr_q, find_kplt_fn, &searchval);
      if ( !addrs ) {
	TEMP_MAP(pdpt_phys, pdpt_virt, PDPTE2vaddr(pml4t_idx, 0));
	kmemset((void*)pdpt_virt, 0, PAGE_SIZE);
	setup_table((void*)pdpt_phys, (void*)(pml4t_virt + (sizeof(union pt_entry)*pml4t_idx)), PG_RW | PG_USER);
      }
      else 
	pdpt_virt = addrs->curr;

      /* find or build the pd */
      searchval = (uint64_t)PDE2vaddr(pml4t_idx, pdpt_idx, 0);
      addrs = (addr_pair*)qsearch(addr_q, find_kplt_fn, &searchval);
      if ( !addrs ) {
	TEMP_MAP(pd_phys, pd_virt, PDE2vaddr(pml4t_idx, pdpt_idx, 0));
	kmemset((void*)pd_virt, 0, PAGE_SIZE);
	setup_table((void*)pd_phys,(void*)(pdpt_virt + (sizeof(union pt_entry)*pdpt_idx)), PG_RW | PG_USER );
      }
      else
	pd_virt = addrs->curr;

      /* find or build the pt */
      searchval = (uint64_t)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0);
      addrs = (addr_pair*)qsearch(addr_q, find_kplt_fn, &searchval);
      if ( !addrs ) {
	TEMP_MAP(pt_phys, pt_virt, PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0));
	kmemset((void*)pt_virt, 0, PAGE_SIZE);
	setup_table((void*)pt_phys,(void*)(pd_virt + (sizeof(union pt_entry)*pd_idx)), PG_RW | PG_USER);	
      }
      else 
	pt_virt = addrs->curr;

      /* finally, setup the new PTE */
      setup_table((void*)todo->paddr,(void*)(pt_virt + (sizeof(union pt_entry)*pt_idx)), PG_RW | PG_GLOBAL);

      /* free this todo record */
      kfree_track(PROCMAN_SITE, todo);
    }

    /* clean up the KPLT bookeeping */
    free_partitions(parts);
    qclose(todo_q);

    /* back to your regularly scheduled program of process creation cleanup... */
#endif

    vmem_free_temp((uint64_t*)special_k_base, special_k - special_k_base); 

    while ( (addrs = qget(addr_q)) ) {
      framearray[addrs->paddr / PAGE_SIZE].free = 0x0;
      framearray[addrs->paddr / PAGE_SIZE].vaddr = (uint64_t*)addrs->vaddr;
      kfree(addrs);
    }

    qclose(addr_q);    
    
#undef TEMP_MAP

    /* the TLB still has those temporary mappings. Get rid of them. */
    flush_tlb(TLB_ALL);
  }
  else{ /* idle proc is the "init" proc so it has the original cr3 target */
    first = 0;
    p->cr3_target = (struct page_map_level_4_table*)read_cr3();
  }
}

uint64_t new_fake_cr3_target( Proc_t * p ) {

  Proc_t *np;
  uint64_t ret;

  np = (Proc_t*)kmalloc_track(PROCMAN_SITE, sizeof(Proc_t));

  np->parent = p;
	
  new_cr3_target( np, -1 );
  ret = (uint64_t)np->cr3_target;

  kfree_track(PROCMAN_SITE, np);

  return ret;
}

/******************************************************************************
 *
 * Function: set_iopl(Proc_t *)
 *
 * Description: Sets the port IO privilege level for a process
 *
 *****************************************************************************/
void set_iopl(Proc_t *p,int pl) {
  if (pl > 3)
    return;
  p->mc.eflags &= EFLAGS_IOPL_MASK;            /* Set to 0             */
  p->mc.eflags |= (pl << EFLAGS_IOPL_OFFSET);  /* Set to desired value */
}

/******************************************************************************
 *
 * Function: clone_proc
 *
 * Description: Implements fork. Clones the process structure and everything
 *              needed to run the process (page tables, etc.)
 *
 *****************************************************************************/
Proc_t *clone_proc(Proc_t *parent) {

  return new_proc(0, 0, PROC_NONE, parent);
}

/******************************************************************************
 *
 * Function: destroy_proc
 *
 * Description: Deals with exit strategies and frees all data.
 *
 *****************************************************************************/
void destroy_proc(Proc_t *p, int estatus) {

  int i;
  struct memory_region *mr;
  
  if ( estatus != SOME_EXEC_CONSTANT ) {
    if ( !((p->status & SIG_EXITED) == SIG_EXITED) )
      update_proc_status(p, estatus, EXITED);
    /* Process Genealogy for actual exiting process */
    kwait_exit(p);
  }
  else /* Process Genealogy for execve to assume identity of fork*/
    kwait_assume(p);
  
  /* Message-passing */
  kmsg_purge(p);

  /* Scheduling */
  ksched_purge(p);

  /* Paging and memory */
  kvmem_unmap_devmem(p);           /* Unmap MMIO so it doesn't get freed */
 
  if ( p->pes_hack_user ) 
    kfree_track(PROCMAN_SITE, (void*)p->pes_hack_user);
  //if ( p->pes_hack_kernel )
  //kfree_track(PROCMAN_SITE, (void*)p->pes_hack_kernel);

  /* free the memory region queue */
  while ( (mr = (struct memory_region*)qget(p->mapped_memory_regions)) ) 
    kfree_track(PROCMAN_SITE, mr);
  qclose(p->mapped_memory_regions);

  /* free argv */
  if ( p->argv ) {
    for ( i = 0; i < p->argc; i++) 
      kfree_track(PROCMAN_SITE, p->argv[i]);
    kfree_track(PROCMAN_SITE, p->argv);
  }

  /* free env */
  if ( p->env ) 
    kfree_track(PROCMAN_SITE, (void*)p->env);

  /* Process lookup */
  lut_remove(p->pid);
  
  /* proc structure and cr3 frame freed by the grim reaper */

  return;
}

/******************************************************************************
 *
 * Function: lut_init()
 *
 * Description: FIXME: Write me
 *
 *****************************************************************************/
static void lut_init() {
  proc_htable = hopen(PLUT_SLOTS);
  return;
}

/******************************************************************************
 *
 * Function: lut_add(Proc_t*)
 *
 * Description: Add a proc_t to the pid-to-proc_t lookup table
 *
 *****************************************************************************/
void lut_add(Proc_t *p) {

  if ( p->pid == IDLE_PROC )
    return;

  hput(proc_htable, p, (char*)&p->pid, sizeof(pid_t));
  return;
}

/******************************************************************************
 *
 * Function: lut_remove(pid_t)
 *
 * Description: Removes a proc from the lookup table.  Does not deallocate the
 *              proc_t; that is up to the caller if necessary.
 *
 *****************************************************************************/
void lut_remove(pid_t tpid) {

  if ( tpid == IDLE_PROC )
    return;

  hremove(proc_htable, is_process, (char*)&tpid, sizeof(pid_t));
  return;
}

/******************************************************************************
 *
 * Function: pid_to_addr(pid_t)
 *
 * Description: Given a pid, return the address of the Proc_t structure or NULL
 *
 *****************************************************************************/
Proc_t *pid_to_addr(pid_t n) {
  Proc_t *p;

  if ( n == IDLE_PROC ) {
#ifdef ENABLE_SMP
    return idleps[this_cpu()];
#else 
    return idleps[0];
#endif
  }

  p = hremove(proc_htable, is_process, (char*)&n, sizeof(pid_t));
  if ( p ) 
    hput( proc_htable, p, (char*)&p->pid, sizeof(pid_t));

  return p;
}

void update_proc_status(Proc_t *p, int info, int new_status) {

  if ( new_status ) 
    p->status = (info << 8) | new_status | UNREPORTED;
  else 
    p->status &= ~UNREPORTED;

  return;
}

/* MANIPULATE MEMORY REGION QUEUE */
void add_memory_region(Proc_t *p, int type, int flags, uint64_t start, 
		       uint64_t end) {

  struct memory_region *region;

  region = (struct memory_region*)kmalloc_track(PROCMAN_SITE, sizeof(struct memory_region));

  region->type  = type;
  region->start = start;
  region->end   = end;
  region->flags = flags;

  qput(p->mapped_memory_regions, region);

  p->heap_region = ( type == HEAP_REGION ? region : p->heap_region); 

  return;
}

