# Copyright <2017> <Scaleable and Concurrent Systems Lab; 
#	          Thayer School of Engineering at Dartmouth College>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy 
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights 
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
# copies of the Software, and to permit persons to whom the Software is 
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#include <asm_linkage.h>
#include <vmx.h>
#include <vmx_utils.h>
#include <mregs.h>
#include "asm.h"

	.text
	
	.section .text.vmexit_trap,"ax",@progbits
	.globl vmexit_trap
	.type vmexit_trap, @function

	.section .text.vcpu_get_last,"ax",@progbits
	.globl vcpu_get_last
	.type vcpu_get_last, @function

	.section .text.hypv_acquire_lock,"ax",@progbits
	.globl hypv_acquire_lock
	.type hypv_acquire_lock, @function

	.section .text.hypv_release_lock,"ax",@progbits
	.globl hypv_release_lock
	.type hypv_release_lock, @function

#This macro saves everything but rbp, which is done for us already
#we clobber rbp after this macro.
.macro PTR_SAVE_CONTEXT
    pushq %rax
    pushq %rcx
    pushq  %rdx
    pushq  %rbx
    pushq  %rsi
    pushq  %rdi
    pushq  %r8
    pushq  %r9
    pushq  %r10
    pushq  %r11
    pushq  %r12
    pushq  %r13
    pushq  %r14
    pushq  %r15
    pushq  %rsp
.endm

#Restore the general purpose registers
.macro PTR_RESTORE_CONTEXT
    popq  %rsp
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %r11
    popq  %r10
    popq  %r9
    popq  %r8
    popq  %rdi
    popq  %rsi
    popq  %rbx
    popq  %rdx
    popq  %rcx
    popq  %rax
.endm
	
.macro SAVE_CONTEXT
    pushq %rax
    pushq %rcx
    pushq  %rdx
    pushq  %rbx
    pushq  %rsi
    pushq  %rdi
    pushq  %r8
    pushq  %r9
    pushq  %r10
    pushq  %r11
    pushq  %r12
    pushq  %r13
    pushq  %r14
    pushq  %r15
    pushq  %rbp
    pushq  %rsp
.endm

#Restore the general purpose registers
.macro RESTORE_CONTEXT
    popq  %rsp
    popq  %rbp	
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %r11
    popq  %r10
    popq  %r9
    popq  %r8
    popq  %rdi
    popq  %rsi
    popq  %rbx
    popq  %rdx
    popq  %rcx
    popq  %rax
.endm
	
	
        ## Note that the hypervisor has interrupts disabled, so this code will
        ## never be interrupted and the registers will never thus be clobbered.
vmexit_trap:
	cli
        ## Save the general-purpose registers to the temporary storage
        ## location. FIXME: Will need to be made multiprocessing-friendly.
        pushq %rbp              # Temporarily use %rbp for storage offset.
	PTR_SAVE_CONTEXT
	RELCALL(hypv_acquire_lock)
	RELCALL(vcpu_get_last)
	movq %rax, %rbp
	PTR_RESTORE_CONTEXT
        movq %rax, AXREG(%rbp)  # %rax
        movq %rbx, BXREG(%rbp)  # %rbx
        movq %rcx, CXREG(%rbp)  # %rcx
        movq %rdx, DXREG(%rbp)  # %rdx
        movq %rsi, SIREG(%rbp)  # %rsi
        movq %rdi, DIREG(%rbp)  # %rdi
        movq %r8, R8REG(%rbp)   # %r8
        movq %r9, R9REG(%rbp)   # %r9
        movq %r10, R10REG(%rbp) # %r10
        movq %r11, R11REG(%rbp) # %r11
        movq %r12, R12REG(%rbp) # %r12
        movq %r13, R13REG(%rbp) # %r13
        movq %r14, R14REG(%rbp) # %r14
        movq %r15, R15REG(%rbp) # %r15
        popq %rax               # %rbp value into %rax
        movq %rax, BPREG(%rbp)  # %rbp
        movq FXDATA(%rbp), %rax # Location for SSE to save.
        fxsaveq (%rax)          # Save SSE data.
	RELCALL(vmexit_handler)
	hlt
	.size vmexit_trap, .-vmexit_trap

        ## Restores a vproc's general-purpose registers, and either launches it
        ## or resumes it based on the value of "launched" (the function
        ## argument, a pointer to vp->launched).

	.section .text.run_vproc,"ax",@progbits
	.globl run_vproc
	.type run_vproc, @function
run_vproc:	
        testl $1, (%rdi)        # Launched?
        jz launch               # No
        ## Restore general-purpose registers.
	RELCALL(vcpu_get_last)
	movq %rax, %rbp
        movq FXDATA(%rbp), %rax # SSE save location
        fxrstor (%rax)          # Restore SSE stuff
        movq AXREG(%rbp), %rax  # %rax
        movq BXREG(%rbp), %rbx  # %rbx
        movq CXREG(%rbp), %rcx  # %rcx
        movq DXREG(%rbp), %rdx  # %rdx
        movq SIREG(%rbp), %rsi  # %rsi
        movq DIREG(%rbp), %rdi  # %rdi
        movq R8REG(%rbp), %r8   # %r8
        movq R9REG(%rbp), %r9   # %r9
        movq R10REG(%rbp), %r10 # %r10
        movq R11REG(%rbp), %r11 # %r11
        movq R12REG(%rbp), %r12 # %r12
        movq R13REG(%rbp), %r13 # %r13
        movq R14REG(%rbp), %r14 # %r14
        movq R15REG(%rbp), %r15 # %r15
        movq BPREG(%rbp), %rbp  # %rbp
	SAVE_CONTEXT
	RELCALL(hypv_release_lock)
	RESTORE_CONTEXT
	vmresume                # Resume!
        ## If we made it here, there was an error launching.
        jmp out
launch:
        movl $1, (%rdi)         # Set launched.
	SAVE_CONTEXT
	RELCALL(hypv_release_lock)
	RESTORE_CONTEXT
        vmlaunch                # Launch!
        ## If we made it here, the launch failed.
        movl $0, (%rdi)         # Unset launched.
out:
        ret
        .size run_vproc, .-run_vproc
