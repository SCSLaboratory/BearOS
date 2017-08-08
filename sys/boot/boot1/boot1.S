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

### boot1.S
### This file is placed at the start of a *slice*, and is called by
### the MBR's 512-byte block. For compatibility reasons, this file is
### also exactly 512 bytes.
###

### Make sure this is linked with -Ttext=0x7c00, which is the address
### that both the MBR gets loaded in from the BIOS, and the address
### where the actual MBR loads this in.
###
### Just as when we're loaded up by the MBR, the drive entry will be
### placed in %dl and our slice from the main table will be placed in
### %si.
###
### Note if you go to disassemble this with objdump, the output will
### almost certainly be wrong for some sections. This is because we
### mix 16-, 32-, and 64-bit code in the same file. This is all very
### clearly delimited by the .code16, .code32, and .code64 designators
### given at those points in this file.

	## Global symbols
	.globl gdt_long

	## Memory locations
	.set BOOT_STACK,0x7bf8  # location of the stack "bottom"...
				# the stack grows down so this is the highest
		                # addr of the stack. It would hit the
				# partition table if it overran.
	.set MEM_LOADED,0x7c00  # Where we're loaded
	.set MEM_DLBL,0x7e00    # Start of disklabel
        .set DLBL_BBASE,0x7e28  # "bbase" in disklabel
	.set PART_TBL,0x7be     # Partition table for the disk
	.set NUM_PART,0x88      # Offset to number of partitions
	## Place to put the available memory. Be sure not to use the
	## first low 0x500 bytes of this (it's the IVT etc). This is a
	## many-entry table, with 24-byte entries, each with the
	## following format:
	## First qword = Base address
	## Second qword = Length of "region" (if 0, ignore the entry).
	## Next dword = Region "type"
	##   * Type 1: Usable (normal) RAM
	##   * Type 2: Reserved - unusable
	##   * Type 3: ACPI reclaimable memory
	##   * Type 4: ACPI NVS memory
	##   * Type 5: Area containing bad memory
	## Next dword = ACPI 3.0 extended attributes bitfield (if 24
	##              bytes are returned, but we increment 24 bytes
	##              regardless).
	##   * Bit 0 of the extended attributes field indicates if the
	##     entire entry should be ignored.
	##   * Bit 1 of the extended attributes field indicates if the
	##     entry is non-volatile (whatever that means).
	## This information was taken from:
	## http://wiki.osdev.org/Detecting_Memory_%28x86%29
	.set MEM_ENTRIES,0x802     # Number of mem table entries
	.set MEMTABLE,0x804        # Mem table

	## Basic page table locations for baby's first page tables
	.set PML4T,0x1000       # Page map level 4 table
	.set PDPT,0x2000        # Page directory pointer table
	.set PDT,0x3000         # Page directory table
	.set PT,0x4000          # Page table

	## Constants
	.set PARTSIZE,0x10      # Size of partition entry in label
	.set MAGIC,0xaa55       # Magic: bootable
	.set EMAGIC,0x534D4150  # e820 Magic
	.set SEL_CODE,0x08      # Code selector
	.set SEL_LCODE,0x08     # Code selector, long mode

	.globl start            # Entry point
	.code16                 # Real mode, for now
start:
	xorw %ax,%ax            # Zero
	movw %ax,%es            # Address
	movw %ax,%ds            #  data
	movw %ax,%ss            # Set up
	movw $MEM_LOADED,%sp    #  stack

mmap.0:
	movw $MEMTABLE-24,%di   # Memory table
	xorl %ebx,%ebx          # Zero
	pushw %si               # Save %si
	movw %ax,%si            # Zero
mmap:
	addw $24,%di            # Set next map entry
	movl $0xe820,%eax       # Get memory map
	movl $24,%ecx           # Buffer size
	movl $EMAGIC,%edx       # Signature
	int $0x15               # Get memory map
	jc mmap.1               # Method #1 of marking finished
	cmpl $EMAGIC,%eax       # Error?
	jne mem_err             # Yes
	incw %si                # Total number of entries
	cmpl $0x0,%ebx          # Method #2 of marking finished
	jne mmap                # Not finished
	## Now that we have the memory map set up, let's see about
	## entering protected mode (in the 32-bit part of file).
mmap.1:
	movw %si,MEM_ENTRIES    # Number of memtable entries
	popw %si                # Restore %si
	jmp postmem

mem_err:
	movw $msg_mem,%si       # "Memory detection error"
	jmp putstr              # Error out

### Output an ASCIZ string to the console via the BIOS.
putstr.0:
	movw $0x7,%bx           # Page:attribute
	movb $0xe,%ah           # BIOS: Display
	int $0x10               #  character
putstr:
	lodsb                   # Get character
	testb %al,%al           # End of string?
	jnz putstr.0            # No
putstr.1:
	jmp putstr.1            # Await reset.

msg_mem:.asciz "Memory detection error"

### The GDT. This table looks retarded:
### ---------------------------------------------------------------
### |0                       15|16                              31|
### ---------------------------------------------------------------
### |           Limit 0:15     |                     Base 0:15    |
### ---------------------------------------------------------------
### |32        39|40         47|48         51|52   55|56        63|
### ---------------------------------------------------------------
### | Base 16:23 | Access Byte | Limit 16:19 | Flags | Base 24:31 |
### ---------------------------------------------------------------
### 
### The bit arithmetic here is horrible. The access byte and flags go
### in backwards from what you'd expect (they are defined bitwise
### starting from 7 and going to 0, so when you specify it you have to
### reverse so it starts at 0 and goes to 7...).
gdt:
	.word 0x0, 0x0, 0x0, 0x0                 # Null entry
	.word 0xffff, 0x0, 0x9a00, 0x00cf        # Code entry (SEL_CODE)
	.word 0xffff, 0x0, 0x9200, 0x00cf        # Data entry
	.word 0xffff, 0x0, 0xfa00, 0x00cf        # User code entry
	.word 0xffff, 0x0, 0xf200, 0x00cf        # User data entry
gdt_48:
	.word .-gdt-1
	.long gdt
### If we're short on space, we can do the following by modifying the
### memory at gdt instead of duplicating it all and save about 20
### bytes in the process.
### NOTE: If this expands/contracts, update GDT_SIZE in constants.h
gdt_long:
	.word 0x0, 0x0, 0x0, 0x0                 # Null entry
	.word 0xffff, 0x0, 0x9a00, 0x00af        # Code entry (SEL_LCODE)
	.word 0xffff, 0x0, 0x9200, 0x00cf        # Data entry
	.word 0xffff, 0x0, 0xfa00, 0x00af        # User code entry
	.word 0xffff, 0x0, 0xf200, 0x00cf        # User data entry
	.word 0x0067, 0x6400, 0x8900, 0x0010     # TSS, depends on MEM_TSS_BASE
	.word 0x0, 0x0, 0x0, 0x0                 # TSS Entry Part 2
gdtlong_48:
	.word .-gdt_long-1
	.long gdt_long

### Do things that we need to enter protected mode.
postmem:
	## Set video mode to 80x25 for basic console.
	mov $3,%ax              # Video mode 80x25x16
	int $0x10               # Interrupt (set video mode)
	mov $0x1003,%ax         # Toggle blinking
	mov $0,%bx              # Blinking disabled
	int $0x10               # Interrupt (set video mode)
loadgdt:
	lgdt gdt_48             # Load gdt
	## Don't set up the selectors yet, they push every data
	## reference 16 bytes higher because, since we aren't yet in
	## protected mode, they aren't "selectors", they are still
	## just real-mode segment offsets!
loadidt:
	## We don't have nearly the amount of space here to set up a
	## desirable interrupt table! Defer it for when we go to the
	## C boot code.
loadtss:
	## Not really interesting to set up the TSS here either,
	## because we have no interrupts. Let the C boot code do this,
	## when it handles interrupts.
protected.0:
	## Now we can set up what will become the selectors, since we
	## have no more data references to make.
	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	cli                     # Disable interrupts
	movl %cr0,%eax          # Control Register 0 to %eax
	orb $0x1,%al            # Set the lowest bit
	movl %eax,%cr0          # %eax to Control Register 0
	ljmp $SEL_CODE,$protected # Protected mode
	.code32
protected:
	## Now we're in protected mode. Set up long mode.
	## Have to re-set the segment selectors here, so they are
	## considered 32 bit (otherwise, any time we set data it will
	## fail). 
	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	movl %cr4,%eax          # Control Register 4 to %eax
	bts $5,%eax             # Set PAE
	bts $7,%eax             # Set pages golbal enable
	movl %eax,%cr4          # %eax to Control Register 4
	movl $0xc0000080,%ecx   # EFER register
	rdmsr                   # EFER to %eax
	bts $8,%eax             # Set IA-32e (long mode)
	bts $11,%eax            # Allow No-EXecute bit
	wrmsr                   # %eax to EFER
paging.0:
	## Set up 64-bit paging -- required before we're actually in
	## long mode. The first 1 MB will be identity-mapped.
	movl $PML4T,%edi        # Page-map level 4 table
	movl %edi,%cr3          #  in base table
	movl $4096,%ecx         # 4 kb*4 count
	xorl %eax,%eax          # Zero
	rep                     # Clear
	stosl                   #  PML4T
paging.1:
	## Set the first entry of each page table level to point to
	## the next level. This handles the pointing of PML4T to PDPT,
	## PDPT to PDT, and PDT to PT.
	movl $3,%ecx            # Count
	movl %cr3,%edi          # Destination (Page tables)
	movl $0x1000,%eax       # Increment
	movl $0x2003,%ebx       # Page present, read/writable
paging.2:
	movl %ebx,(%edi)        # Point to next paging level
	addl %eax,%ebx          # Next level
	addl %eax,%edi          # Next level
	loop paging.2           # For each page level
paging.3:
	## Set each page of the PT level to be read/writable. Note
	## that %edi now points to the PT level thanks to the loop.
	movl $0x00000103,%ebx   # Page present, read/writable
	movl $512,%ecx          # Number of pages in PT level
paging.4:
	movl %ebx,(%edi)        # Set present, read/writable
	addl $0x1000,%ebx       # Next 4096 bytes
	addl $8,%edi            # Next page entry
	loop paging.4           # For each page entry
paging.5:

Recursive.Paging:			# Recursive paging is set up here
	movl $PML4T, %eax		# Move phys address of PML4T into eax
	addl $0x1000, %eax		# Add Page Size to move to end of PML4T
	subl $0x8, %eax			# Move to last entry of PML4T
	
	movl $PML4T, %ebx		# Move Phys address of PML4T into ebx
	addl $0x3, %ebx			# Add page permission bits Present+RW
	mov %ebx, (%eax)		# Map into last entry of PML4T

	##Turn on paging
	movl %cr0,%eax          # Control Register 4 to %eax
	orl $0x80000000,%eax    # Enable paging
	movl %eax,%cr0          # %eax to Control Register 4
	lgdt gdtlong_48         # Load long-mode gdt
	ljmp $SEL_LCODE,$long   # Enter long mode
	.code64
long:
	##  Welcome to long mode.
	## As above, reload the segment registers.
	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	## Enable SSE (required for stuff like clang varargs), and other
	## things that should be turned on.
	movq %cr0,%rax          # Control register 4 to %eax
	bts $1,%rax             # Set MP bit
	btr $2,%rax             # Clear EM bit
	bts $5,%rax             # Allow native (new) FPU error reporting
	movq %rax,%cr0          # %eax to control register 0
	movq %cr4,%rax          # Control register 4 to %eax
	bts $9,%eax             # Set OSFXSR bit
	bts $10,%eax            # Set OSXMMEXCPT bit
	movq %rax,%cr4          # %eax to control register 4
	## Now let's return to C and be done with this nonsense.
        ## The boot block base is loaded in as part of the disklabel,
        ## and we know the disklabel starts right after us in the code.
        ## So, find the boot block base and jump there.
        movq DLBL_BBASE,%rbx    # Boot block base (bytes)
        addq $MEM_DLBL,%rbx     # Add start of disk label
        subq $0x200,%rbx        # Subtract the first 512 bytes (included in
                                #  disklabel for some reason)
	movq $BOOT_STACK,%rsp   # Reset the stack pointer 
	jmp *%rbx               # Into the C entry point

### Fill the rest of the 512 bytes with NOP and make bootable.
	.org 0x1FE,0x90         # Fill the rest with NOPs
	.word MAGIC             # Bootable magic
