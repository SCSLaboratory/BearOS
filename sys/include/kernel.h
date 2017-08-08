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
 *
 * Filename: kernel.h
 *
 * Description: Includes panic function, kernel config macros, etc.
 *
 *****************************************************************************/

#include <constants.h>  /* For PL_x      */
#include <proc.h>       /* For Proc_t    */
#include <elf_loader.h>

/******************************************************************************
 * DEFINES ********************************************************************
 *****************************************************************************/



/* Segments and their associated privilege level */
#define K_CODESEG (0x8  | PL_0)
#define K_DATASEG (0x10 | PL_0)
#define U_CODESEG (0x18 | PL_3)
#define U_DATASEG (0x20 | PL_3)


/******************************************************************************
 * PUBLIC FUNCTIONS ***********************************************************
 *****************************************************************************/

/* Prints the given error string and halts the system. */
void kpanic(char *);

Proc_t *kernel_exit();

/* Handles the PIT timer interrupt. */
void systick_handler(unsigned int, void*);
typedef void(*systick_hook)(void);
void systick_hook_add(systick_hook);
void systask_hook_remove(systick_hook);

/* Converts an interrupt into a message to the given process */
void interrupt(unsigned int, void*);

/* Loads a binary file into a process's memspace. */
int load_elf_proc(void); /* Get bin off of disk; load  */

/* funciton to search a queue of procs by pid. */
int is_process(void*,const void*);

/* Sets the permissions for the vgad process so it can see VGA mem */
void set_vgad_perms(Proc_t *p);

/* Sets the permissions for the kbd process so it can see kbd hardware */
void set_kbd_perms(Proc_t *p);

/* First routine to be called by new_proc to start the process */
void kstart();

