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

#include <constants.h>                  /* For  and static            */
#include <proc.h>                       /* For Proc_t                        */
#include <memory.h>                     /* For paging structs                */
#include <sbin/syspid.h>
#include <khash.h>

/* Used by procman - starting values for the PID counters */
#define USER_PID_START 1

pid_t pid_counter;
pid_t *user_pid_counter;     /* what pid the next new user proc gets   */
uint64_t number_of_issued_pages; /*tracks how many pages umalloc currently has issued*/


/******************************* INIT SCHEDULER ******************************/
 void procman_init();              /* Set up queues, etc                */

/************************* ALLOCATE/DESTROY PROCESSES ************************/
Proc_t *new_proc(uint64_t,int,pid_t,Proc_t*); /* allocate new Proc_t */
Proc_t *new_kernel_proc(uint64_t, pid_t);
Proc_t *clone_proc(Proc_t*);        /* a la fork()                     */
void destroy_proc(Proc_t*,int);     /* deallocate everything in there  */
uint64_t new_fake_cr3_target(Proc_t*); /* make a new CR3 target without overwriting the old one - Diversity */

/************************** PROCESS REFRESH **********************************/
 void *refresh_proc(void *vp);

/************************* VIRTUAL MEMORY MODIFICATION ***********************/
 void update_pml4t(Proc_t *p,int);
 void restore_proc_vmem(Proc_t*);  /* Put proc's memspace into vmem     */
 void put_scratch(Proc_t*,int);    /* Put proc's memspace into scratch  */
 void clear_scratch(int);          /* Clear scratch area                */
/* Interface for adding pages to processes for the ELF loader. */
void *elf_add_page(Proc_t *, uint64_t, uint64_t, int, int, int);
/* Interface for cloning pages for the diversifier. */
 void diversity_clone_page(void *, union pt_entry *, uint64_t, uint64_t);
/* Interface for copying pages for the diversifier. */
 void diversity_copy_page(void *, uint64_t, uint64_t);

/****************************** PROC_T LOOKUP ********************************/
 Proc_t *pid_to_addr(pid_t);

void update_proc_status(Proc_t *, int, int);


hashtable_t *get_proc_lut();

void add_memory_region(Proc_t *p, int type, int flags, uint64_t start, uint64_t end);

void lut_add(Proc_t*);           /* Add a proc to the lut              */
void lut_remove(pid_t);          /* Remove a proc from the lut         */
