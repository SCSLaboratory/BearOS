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

#ifdef KERNEL 

#include <constants.h>
#include <proc.h>

int     ksched_init    ();          /* Init module */
Proc_t *ksched_get_last();          /* Proc that ran before kernel */
Proc_t *ksched_schedule();          /* Run the scheduling algorithm */
void    ksched_block   (Proc_t *p); /* Keep given process from running */
void    ksched_unblock (Proc_t *p); /* Allow given process to run */
void    ksched_add     (Proc_t *p); /* Add proc to scheduler queue */
void    ksched_purge   (Proc_t *p); /* Purge proc from scheduler queue */
void    ksched_ps(void *rp);	    /* print blocked processes */

/* Scheduler hooks. */
typedef void(*ksched_hook)(Proc_t *);
void ksched_hook_add(ksched_hook);
void ksched_hook_remove(ksched_hook);

/* print the relations of p */
void ksched_printrelations(Proc_t *p);
/* record a process in the ps response */
void ksched_save_entry(char status,Ps_resp_t *rp,Proc_t* p); 

void ksched_yield(void);

Proc_t **proc_ptr_array;
Proc_t **idleps;                 /* For when all procs are blocked */

#endif
