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
/*
 * swint.S
 *
 * This file contains the swints in the truest sense of the word. That is,
 * any function that traps to the kernel goes through this file.  Note that
 * this does not include a number of library functions which people sometimes
 * lump under the heading "swints."
 *
*/

#include <asm_linkage.h>
#include <swint.h>


.section .note.GNU-stack,"",%progbits

	/* SYSTEM CALL CONVENTION
	 * - pid of target process (src or dest of msg) is in %rdi (first argument)
	 * - Pointer to message is in %rsi (second argument)
	 * - Flag indicating the message type is in %rdx (third argument) */
	ENTRY(swint)
	xorq %rax, %rax         /* retval is 0 unless kernel says different */
	int $0x80               /* Interrupt */
	ret
	SET_SIZE(swint)

	ENTRY(swint_1)
	xorq %rax, %rax         /* retval is 0 unless kernel says different */
	int $0x81               /* Interrupt */
	ret
	SET_SIZE(swint_1)

	ENTRY(swint_2)
	xorq %rax, %rax
	int $0x7D
	ret
	SET_SIZE(swint_2)

	ENTRY(swint_3)
	xorq %rax, %rax
	int $0x7F
	ret
	SET_SIZE(swint_3)

	ENTRY(swint_4)
	xorq %rax, %rax
	int $0x7C
	ret
	SET_SIZE(swint_4)

	ENTRY(swint_5)
	xorq %rax, %rax
	int $0x7B
	ret
	SET_SIZE(swint_5)
