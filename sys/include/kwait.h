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
/******************************************************************************
 * Filename: kwait.c
 *
 * Description:
 *  API for making the wait() and waitpid() syscalls happen.
 *
 *****************************************************************************/

#include <constants.h>
#include <proc.h>

/* Init function */
int kwait_init();

/* Called when a process needs to wait */
int kwait_wait(pid_t, pid_t, int, void (*callback_func)(Proc_t*,int,int,unsigned int),unsigned int);

/* Called whenever a new process is created */
void kwait_new(pid_t, pid_t);

/* Called whenever an exit happens */
void kwait_exit(Proc_t *p);

/* Called whenever execve assumes a pid from fork */
void kwait_assume(Proc_t *p);

/* wakes the reaper process to harvest the life of a dead process */
void reap_proc( Proc_t *p );
