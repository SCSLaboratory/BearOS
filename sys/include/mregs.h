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
*
* File: mregs.h
* Lists the machine registers necessary to context switch, along with their
* offsets into the Proc_t structure.
*
* File borrows heavily from the context switch ideas in MINIX, but targets
* the code for x86-64.
*
*/

	W = 8

/* Offsets in struct proc. They MUST match proc.h. */
	P_STACKBASE = 0
	AXREG = P_STACKBASE
	BXREG = AXREG+W
	CXREG = BXREG+W
	DXREG = CXREG+W
	SIREG = DXREG+W
	DIREG = SIREG+W
	R8REG = DIREG+W
	R9REG = R8REG+W
	R10REG = R9REG+W
	R11REG = R10REG+W
	R12REG = R11REG+W
	R13REG = R12REG+W
	R14REG = R13REG+W
	R15REG = R14REG+W
	PCREG  = R15REG+W
	CSREG  = PCREG+W	
	EFLREG = CSREG+W
	SPREG  = EFLREG+W
	SSREG  = SPREG+W
	BPREG  = SSREG+W
	FXDATA = BPREG+W /* FPU/MMX/SSE; 512 bytes */

