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

#ifndef NULL
#define NULL ((void *)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define PAGE_SHIFT 12
#define PAGE_SIZE 4096

/* A failure condition for many functions related to memory. */
#define MEM_FAIL (~0UL)

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than PAGE_SIZE.
 */

#define MSIZE           256             /* size of an mbuf */
#define MCLSHIFT        11              /* convert bytes to mbuf clusters */
#define MCLBYTES        (1 << MCLSHIFT) /* size of an mbuf cluster */

#if PAGE_SIZE < 2048
#define MJUMPAGESIZE    MCLBYTES
#elif PAGE_SIZE <= 8192
#define MJUMPAGESIZE    PAGE_SIZE
#else
#define MJUMPAGESIZE    (8 * 1024)
#endif

#define MJUM9BYTES      (9 * 1024)      /* jumbo cluster 9k */
#define MJUM16BYTES     (16 * 1024)     /* jumbo cluster 16k */


/* ---------------------- Various locations in memory ---------------------- */
#define MEMORY_MAP         0x804 /* BIOS memory map */
#define MEMORY_MAP_ENTRIES 0x802 /* Number of entries in memory map */
#define INIT_PAGE_PML4T 0x1000   /* Initial page map level 4 table */
#define INIT_PAGE_PDPT  0x2000   /* Initial page directory pointer table */
#define INIT_PAGE_PDT   0x3000   /* Initial page directory table */
#define INIT_PAGE_PT    0x4000   /* Initial page table */
#define BOOT_SCRATCH_BEGIN 0x5000 /* Start of bootloader scratch region */
#define BOOT_OTHERPTS   0x100000 /* Other boot-loader-defined page tables */
#define BOOT_NUMPTS     5        /* Number of other boot-loader page tables */
#define BOOT_NUMPTSVAR  0x5008   /* Variable version of BOOT_NUMPTS */
#define BOOT_KERNEL     0x800000 /* Start of memory to put the kernel in */
#define BOOT_MEMLIMIT   0xC00000 /* End of boot loader memory; 12 MB */
#define KERNEL_PAGE_PML4T 0x8000001000       /* Address after memory is setup */

/* Note that the MMAP begins 32 megabytes after the hypervisor. Thus, a size
 * limit for the hypervisor binary is 32 megabytes. */
#define HYPERVISOR_BASE 0xFF8000000000       /* Address to load hypervisor */
#define HYPERVISOR_MMAP 0xFF8002000000       /* Address of memory table */

#define TEST_LOCATE 0x800000F510


#define MEM_HYPV_BASE   0x20000000000 /* Hypervisors logical memory location */

/* -------------------------------- MSRs ---------------------------------- */
#define IA32_TIME_STAMP_COUNTER         0x10
#define IA32_FEATURE_CONTROL            0x3a
#define IA32_PLATFORM_INFO 		0xCE
#define IA32_VMX_BASIC                  0x480
#define IA32_VMX_PINBASED_CTLS          0x481
#define IA32_VMX_PROCBASED_CTLS         0x482
#define IA32_VMX_EXIT_CTLS              0x483
#define IA32_VMX_ENTRY_CTLS             0x484
#define IA32_VMX_MISC                   0x485
#define IA32_VMX_CR0_FIXED0             0x486
#define IA32_VMX_CR0_FIXED1             0x487
#define IA32_VMX_CR4_FIXED0             0x488
#define IA32_VMX_CR4_FIXED1             0x489
#define IA32_VMX_PROCBASED_CTLS2        0x48b
#define IA32_VMX_TRUE_PINBASED_CTLS     0x48d
#define IA32_VMX_TRUE_PROCBASED_CTLS    0x48e
#define IA32_VMX_TRUE_EXIT_CTLS         0x48f
#define IA32_VMX_TRUE_ENTRY_CTLS        0x490
#define IA32_VMX_VMFUNC                 0x491
 
/* --------------------- Other interesting constants ---------------------- */
#define SCRATCH1_IDX 2
#define SCRATCH2_IDX 3
#define SCRATCH1_OFFSET 0x10000000000
#define SCRATCH2_OFFSET 0x18000000000
#define LEGACY_VGA_MEM ((unsigned short *)0x80000b8000)
#define VGA_MEM_SIZE 0x8000
#define PHYS_VGA_MEM 0xb8000
#define STD_VGA_COLOR   3              /* Standard color for VGA printing    */
#define KSTACK_PAGES 4                 /* # of pages allocated for stack */
#define GDT_SIZE 56

/* --------------- Segment selector/privilege level constants ------------- */
#define PL_3 0x3
#define PL_2 0x2
#define PL_1 0x1
#define PL_0 0x0

/* -------------------------- FLAGS register bits --------------------------- */
/* Reserved bits */
#define EFLAGS_BASE (0x2)
/* Bit flags */
#define EFLAGS_ID   (1 << 21)
#define EFLAGS_VIP  (1 << 20)
#define EFLAGS_VIF  (1 << 19)
#define EFLAGS_AC   (1 << 18)
#define EFLAGS_VM   (1 << 17)
#define EFLAGS_RF   (1 << 16)
#define EFLAGS_NT   (1 << 14)                /* Nested Task Flag (0)         */
#define EFLAGS_IF   (1 <<  9)                /* Interrupt Enable Flag        */
#define EFLAGS_TF   (1 <<  8)                /* Trap Flag                    */
#define EFLAGS_IOPL_MASK 0x3FCFFF            /* Port I/O Privilege Level Mask*/
#define EFLAGS_IOPL_OFFSET 12
#define eflags_iopl(pl) (pl << EFLAGS_IOPL_OFFSET)  /* Port I/O Priv. Level  */

/* --------- Standard permission bits for read, write, and execute -------- */
#define PERM_READ  0x1
#define PERM_WRITE 0x2
#define PERM_EXEC  0x4

/* --------------------- coding style and compatibility support ------------*/
#define TRUE 1
#define FALSE 0
