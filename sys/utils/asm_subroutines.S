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
#include <asm_subroutines.h>
#include <mregs.h>

.section .note.GNU-stack,"",%progbits

/* Note: The first integer parameter is %rdi.
 *       The integer return value is %rax. */
/*************************** PORT IO IN ***************************************/
	ENTRY(inb)
	xorq %rax,%rax
	movq %rdi,%rdx
	inb %dx,%al
	ret
	SET_SIZE(inb)

	ENTRY(inw)
	xorq %rax,%rax   /* clear rax */
	movq %rdi,%rdx   /* move rdi into rdx */
	inw %dx,%ax      /* return lower 16 */
	ret
	SET_SIZE(inb)
	
	ENTRY(inl)
	xorq %rax,%rax
	movq %rdi,%rdx
	inl %dx,%eax
	ret
	SET_SIZE(inb)

	ENTRY(insw)
	movq %rdx,%rcx	
	movq %rdi,%rdx
	movq %rsi,%rdi
	rep 
	insw
	ret
	SET_SIZE(insw)	


/************************** PORT IO OUT ***************************************/

	ENTRY(outb)
	xorq %rax,%rax
	movq %rdi,%rdx
	movq %rsi,%rax
	outb %al,%dx
	ret
	SET_SIZE(outb)
	
	ENTRY(outw)
	xorq %rax,%rax
	movq %rdi,%rdx
	movq %rsi,%rax
	outw %ax,%dx
	ret
	SET_SIZE(outb)

	ENTRY(outl)
	xorq %rax,%rax
	movq %rdi,%rdx
	movq %rsi,%rax
	outl %eax,%dx
	ret
	SET_SIZE(outb)

/*************************** READ SPECIAL REGISTERS ***************************/

	ENTRY(cpuid)
	pushq %rbx
	movq %rdi,%rax
	cpuid
	testq %rsi,CPUID_EDX
	jnz cpuid_edx
	movq %rcx,%rax
	jmp cpuid_out
cpuid_edx:
	movq %rdx,%rax
cpuid_out:
	popq %rbx
	ret
	SET_SIZE(cpuid)

	ENTRY(read_cr0)
	movq %cr0,%rax
	ret
	SET_SIZE(read_cr0)

	ENTRY(read_cr1)
	movq %cr1,%rax
	ret
	SET_SIZE(read_cr1)

	ENTRY(read_cr2)
	movq %cr2,%rax
	ret
	SET_SIZE(read_cr2)

	ENTRY(read_cr3)
	movq %cr3,%rax
	ret
	SET_SIZE(read_cr3)

	ENTRY(read_cr4)
	movq %cr4,%rax
	ret
	SET_SIZE(read_cr4)

	ENTRY(read_msr)
	movq %rdi,%rcx
	rdmsr
	/* rdmsr puts the 64-bit value into edx:eax, so move the upper 32 bits
	 * (%eax) into the upper 32 bits of %rax. */
	shlq $32,%rdx
	orq %rdx,%rax
	ret
	SET_SIZE(read_msr)

/************************** WRITE SPECIAL REGISTERS ***************************/
	ENTRY(write_cr0)
	movq %rdi, %cr0
	ret
	SET_SIZE(write_cr0)
	
	ENTRY(write_cr3)
	movq %rdi, %cr3
	ret
	SET_SIZE(write_cr3)

	ENTRY(write_cr4)
	movq %rdi, %cr4
	ret
	SET_SIZE(write_cr4)

        ENTRY(write_msr)
        movq %rdi,%rcx          /* MSR value is first parameter */
        movl %esi,%eax          /* Lower 32 bits of MSR to %eax */
        shrq $32,%rsi           /* Upper 32 bits */
        movl %esi,%edx          /*  to %edx */
        wrmsr                   /* Write */
        ret
        SET_SIZE(write_msr)

/********************************** PANIC *************************************/

	ENTRY(panic)
phalt:
	cli
	hlt
	jmp phalt
	SET_SIZE(panic)

/********************************** IDLE **************************************/
	ENTRY(idle)
#	movq %rdi, %rsp
ihalt:
	#cli
	sti
	hlt
	jmp ihalt
	SET_SIZE(idle)

/******************************* RELOAD SEGS **********************************/
	
	ENTRY(reload_segs)
	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	movw $0x28,%ax
	ltr %ax                 #  tss
	ret
	SET_SIZE(reload_segs)
