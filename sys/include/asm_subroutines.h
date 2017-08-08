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
#ifndef _IN_ASM
#include <stdint.h>

uint64_t cpuid(uint64_t, uint64_t);
uint64_t read_cr0();
uint64_t read_cr1();
uint64_t read_cr2();
uint64_t read_cr3();
uint64_t read_cr4();
uint64_t read_msr(uint32_t);
void write_cr0(uint64_t);
void write_cr3(uint64_t);
void write_cr4(uint64_t);
void write_msr(uint32_t, uint64_t);
void panic();

#endif /* _IN_ASM */

/* Flags for CPUID. CPUID_ECX returns the contents of the ECX register, and
 * CPUID_EDX returns the contents of the EDX register.
*/
#define CPUID_ECX 0
#define CPUID_EDX 1

/* CPUID features enumeration */
#define CPUID_FEATURE_VMX (1 << 5)
#define CPUID_FEATURE_APIC (1 << 9)
