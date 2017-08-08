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
#include <asm_linkage.h>
#include <sbin/pio.h>

.section .note.GNU-stack,"",%progbits

#ifdef USER

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
#endif
