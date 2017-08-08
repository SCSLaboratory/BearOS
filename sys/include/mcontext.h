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
* mcontext.h
*
* Defines the structs necessary for keeping around the machine
*  context of a process while it is off the CPU.
*
* Inspired by MINIX, although updated to run
* only on the x86-64 platform.
*
* Right now there's only GP-register state defined.  Eventually there will be
* FPU, MMX, AES, etc.
*/

#include "stdint.h"

typedef uint64_t reg_t;

struct mcontext {
	reg_t rax;  /* Actual register values rax - rbp */  
	reg_t rbx;
	reg_t rcx;
	reg_t rdx;
	reg_t rsi;
	reg_t rdi;
	reg_t r8;
	reg_t r9;
	reg_t r10;
	reg_t r11;
	reg_t r12;
	reg_t r13;
	reg_t r14;
	reg_t r15;   
	reg_t rip;   
	reg_t cs;    
	reg_t eflags;
	reg_t rsp;   
	reg_t ss;    
	reg_t rbp;   
	char *sse;  /* Pointer to sse save space */
} __attribute__ ((packed));


