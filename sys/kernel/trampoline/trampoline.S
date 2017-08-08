.globl trampoline_start
.text
.code16   
       
trampoline_start:
	cli
	xorw    %ax,%ax
	movw    %ax,%ds
	movw    %ax,%es
	movw    %ax,%ss
	movw $0x6000,%sp    #  stack

	lgdt gdt_48

	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	movl %cr0,%eax          # Control Register 0 to %eax
	orb $0x1,%al            # Set the lowest bit
	movl %eax,%cr0          # %eax to Control Register 0
	ljmp $0x08,$protected # Protected mode
	.code32

protected:
	## Now we're in protected mode. Set up long mode.
	## Have to re-set the segment selectors here, so they are
	## considered 32 bit (otherwise, any time we set data it will
	## fail). I'm not sure if this is a bug in QEMU or if it works
	## this way on bare hardware as well.
	movw $0x10,%ax          # Set segment selectors
	movw %ax,%ds            #  data
	movw %ax,%ss            #  stack
	movw %ax,%es            #  es
	movw %ax,%fs            #  fs
	movw %ax,%gs            #  gs
	movl %cr4,%eax          # Control Register 4 to %eax
	bts $5,%eax             # Set PAE
	movl %eax,%cr4          # %eax to Control Register 4
	movl $0xc0000080,%ecx   # EFER register
	rdmsr                   # EFER to %eax
	bts $8,%eax             # Set IA-32e (long mode)
	bts $11,%eax            # Allow No-EXecute bit
	wrmsr                   # %eax to EFER


## added ##

	movl $0x1000,%edi        # Page-map level 4 table
	movl %edi,%cr3          #  in base table

## end added ##

	movl %cr0,%eax          # Control Register 4 to %eax
	orl $0x80000000,%eax    # Enable paging
	movl %eax,%cr0          # %eax to Control Register 4
	lgdt gdtlong_48         # Load long-mode gdt
	ljmp $0x08,$long   # Enter long mode
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
        movq 0x7e28,%rbx    # Boot block base (bytes)
        addq $0x7e00,%rbx     # Add start of disk label
        subq $0x200,%rbx        # Subtract the first 512 bytes (included in
                                #  disklabel for some reason)
	movq $0x7bf8,%rsp       # Reset the stack pointer to a value clang likes
	jmp *%rbx               # Into the C entry point

spin:
	jmp spin

gdt:
	.word 0x0, 0x0, 0x0, 0x0                 # Null entry
	.word 0xffff, 0x0, 0x9a00, 0x00cf        # Code entry (SEL_CODE)
	.word 0xffff, 0x0, 0x9200, 0x00cf        # Data entry
	.word 0xffff, 0x0, 0xfa00, 0x00cf        # User code entry
	.word 0xffff, 0x0, 0xf200, 0x00cf        # User data entry
gdt_48:
	.word .-gdt-1
	.long gdt

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
trampoline_end:


