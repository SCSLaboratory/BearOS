### The MBR (stage 0) of the bootloader. This was partially adapted from
### the FreeBSD file of the same name.
###
### Copyright (c) 2011 Morgon Kanter (bear system)
### Copyright (c) 1999 Robert Nordier
### All rights reserved.
### Redistribution and use in source and binary forms are freely
### permitted provided that the above copyright notice and this
### paragraph and the following disclaimer are duplicated in all
### such forms.
###
### This software is provided "AS IS" and without any express or
### implied warranties, including, without limitation, the implied
### warranties of merchantability and fitness for a particular
### purpose.

### Bear's boot system / disk partitioning goals:
### - The number one goal is compatibility with BSD
### - The number two goal is compatibility with the rest of the world.
### Number one takes precedent over number two.
###
### This file is compiled into a block exactly 512 bytes long, which is placed
### exactly at the start of the hard drive (or partition, in a chain-loading
### scheme). The last two bytes of this block _must_ be 0x55 0xAA -- this is a
### requirement of "IBM compatible BIOS". That's it, though -- after that, all
### bets are off. The BIOS reads this block and starts executing at the first
### byte. However, as part of IBM compatibility we have the next 31.5 kilobytes
### downstream as the "DOS compatible region", which is left for us to do what
### we choose (in this case, load more code). For example, GRUB calls the 512
### byte and 31.5 kilobyte blocks "stage1" and "stage1.5", respectively. For us
### we do what FreeBSD does and have a "boot1", "boot2", and "boot3" after this
### For more information on all this, see http://www.pixelbeat.org/docs/disk/.
###
### MS DOS uses the first 62 bytes as a "parameter block", other systems do
### other things with the region. We do no such thing. We use the entire 512
### bytes for code, with the exception of an IBM-compatible partition table
### from 0x1BE - 0x1FD (64 bytes).


###      BOOT BLOCK STRUCTURE
###
### This code implements a Master Boot Record (MBR) for an Intel/PC disk.
### It is 512 bytes long and it is normally loaded by the BIOS (or another
### bootloader) at 0:0x7c00. This code depends on %cs:%ip being 0:0x7c00
###
### Unlike the default FreeBSD MBR, this is a simplified version that merely
### boots the first bootable partition in the partition table.
###
### As in every Master Boot Record, the partition table is at 0x1be,
### made of four 16-byte entries each containing:
###
###   OFF SIZE   DESCRIPTION
###    0   1     status (0x80: bootable, 0: non bootable)
###    1   3     start sector CHS
###              8:head, 6:sector, 2:cyl bit 9..8, 8:cyl bit 7..0
###    4   1     partition type
###    5   3     end sector CHS
###    8   4     LBA of first sector
###   12   4     partition size in sectors
###
### and followed by the two bytes 0x55, 0xAA (MBR signature).


###      CONSTANTS
### LOAD is the original load address where the BIOS puts our code and cannot
###      be changed.
### 
### EXEC is the relocation address. If you change it, you also need to change
###      the value passed to the linker in the Makefile. REMEMBER TO SET THE
###      VALUE OF -Ttext GIVEN TO THE LINKER TO THIS VALUE.
### 
### PRT_OFF is the location of the partition table (from the MBR standard).
### 
### MAGIC is the signature of the MBR block, which must be at the end.
###
### NHRDRV is the address in segment 0 where the BIOS writes the total number
###        of hard disks in the system.
###
### DLBL_OFF is the start of the disklabel in memory, when it's pulled from the
###          disk. No matter where the partition is on disk, this will still be
###          at the same place in memory.
### 
### DLBL_BBLOCK is the part of the disklabel that defines the offset to boot
###             block. For more information on the structure of the disklabel,
###             see <include/disklabel.h>.
###
### DLBL_NPART is the number of partitions contained in the label.

	.set LOAD,0x7c00        # Load (on boot) address
	.set EXEC,0x600         # Execution (relocate) address
	.set PT_OFF,0x1be       # Partition table
	.set MAGIC,0xaa55       # Magic: bootable
	.set NHRDRV,0x475       # Number of hard drives (address),
	                        #   stored by BIOS
	.set DLBL_OFF,0x7e00    # Start of disk label, once loaded
        .set DLBL_PBASE,0x7e30  # End of boot block (bbsize = pbase - bbase)
        .set SLICE_OFFSET,0x800 # Where to store the offset to the loaded slice
	 
	.globl start            # Entry point
	.code16                 # Run in real mode

start:
	cld                     # String ops go forward
	xorw %ax,%ax            # Zero
	movw %ax,%es            # Address
	movw %ax,%ds            #  data
	movw %ax,%ss            # Set up
	movw $LOAD,%sp          #  stack
	## Relocate ourself to a lower address so that we are out of
	## the way when we load in the bootstrap from the partition to
	## boot.
	movw $main-EXEC+LOAD,%si # Source
	movw $main,%di           # Destination
	movw $0x200-(main-start),%cx # Byte count
	rep                          # Relocate
	movsb                        #  code
	## Jump to the relocated code.
	jmp main-LOAD+EXEC

### Scan the partition table looking for an active entry. Note that
### %ch is zero from the repeated string instruction above. We save
### the offset of the active partition in %si and scan the entire
### table to ensure that only one partition is marked active.
main:
	xorw %si,%si            # No active partition
	movw $partbl,%bx        # Partition table
	movb $0x4,%cl           # Number of entries
main.1:
	cmpb %ch,(%bx)          # Null entry?
	je main.2               # Yes
	jg err_pt               # If 0x1...0x7f
	testw %si,%si           # Active already found?
	jnz err_pt              # Yes
	movw %bx,%si            # Point to active
main.2:
	addb $0x10,%bl          # Till
        loop main.1             #  done
	testw %si,%si           # Active found?
	jnz main.3              # Yes
	int $0x18               # BIOS: Diskless boot
### Ok, we've found an active slice (partition in IBM-compat
### parlance). Check to see that the drive is a valid hard drive
### number. Note that when this code is loaded (from BIOS int 0x19),
### %dl will be set to 0x00 if it's loaded from the first floppy and
### 0x80 if it's loaded from the first hard disk.
main.3:
	cmpb $0x80,%dl          # Drive valid?
	jb main.4               # No
	movb NHRDRV,%dh         # Calculate the highest
	addb $0x80,%dh          #  drive number available
	cmpb %dh,%dl            # Within range?
	jb main.5               # Yes
main.4:
	movb (%si),%dl          # Load _first_ drive (0x80)
### Okay; now that we have a valid drive and partition entry, read the
### disk. We use the "new" EDD stuff instead of the old unreliable CHS
### technique. Any system we care about has EDD already installed
### anyway. Still, test that it's there and error out if not.
main.5:
	movw %sp,%di            # Save stack pointer
	movw $LOAD,%bx          # Transfer buffer
	pushw %bx               # Save %bx (loaded code location)
	movw $0x55aa,%bx        # Magic
	movb $0x41,%ah          # BIOS: EDD extensions
	int $0x13               #  present?
	jc err_edd              # No
	cmpw $0xaa55,%bx        # Magic ok?
	jne err_edd             # No
	testb $0x1,%cl          # Packet mode present?
	jz err_edd              # No
	pushw %si               # Save %si
	addw $0x8,%si           # Read at this sector.
	movw $0x2,%bx           # Read two sectors
	callw read
main.6:
	popw %si                # Restore %si (selected partition)
        pushw %si               # And save it again.
        addw $0x8,%si           # Starting sector
        movw (%si),%si          # Save sector offset to loaded slice..
        movw %si,SLICE_OFFSET   #  to memory
        popw %si                # Restore %si (selected partition)
### Now we've read in the first two sectors of the given partition,
### which gives us (A) the MBR for that partition (512 bytes) and (B)
### the part of the disklabel we care about. From this, we calculate
### how much we *actually* will need to read in so we have every
### single piece of the boot code, and then read that in. That piece
### of information is placed in the boot block at offset 138; however,
### that's the number of *bytes*, not sectors. So we divide that by
### 512; the number in that location *must* be divisible by 512! (Be
### sure to round up.) There is a ceiling on the number of sectors we
### can read, so that divided value will fit into %bx. Note that %si
### still contains the selected partition.
###
### Just to be clear, we need to read in this much of the disk:
###  * The disklabel (200 bytes)
###  * The partition table immediately following the disklabel (variable bytes)
###    -> These two together are given as "total size" in the disklabel
###  * The region up to the boot block (bbase - total_size)
###  * The boot block itself (pbase - bbase)
### Note that we have to read everything up until bbase (the boot block) anyway,
### so we can just skip adding up all the parts. Also note that the first
### allocable byte is just the end of the boot block, so all we have to do is
### read up until pbase -- conveniently, that's exactly the size of what we
### have to read!
main.7:
        pushl %edx              # Save %edx (booted hard drive)
        movl DLBL_PBASE,%eax    # First allocable byte (size of disk label)
        xorl %edx,%edx          # Top half of quotient is zero
        movl $512,%ecx          # Sector size
        divl %ecx               # Number of blocks in partition table
        movl %eax,%ebx          # Sectors to read
        testl %edx,%edx         # Remainder?
        jz main.8               #  No
        incl %ebx               # Add one more block for the remainder
main.8:
        popl %edx               # Restore %edx (booted hard drive)
	pushw %si               # Save %si
	addw $0x8,%si           # Read at this sector
	callw read              # Read sectors
main.9:
	popw %si                # Restore %si (selected partition)
	popw %bx                # Restore %bx (loaded code location)

### Enable a20 so we can access memory above 1 meg.
### Use the zero-valued %cx as a timeout for embedded hardware which
### do not have a keyboard controller.
seta20:
        xorw %cx,%cx            # Zero %cx
	cli                     # Disable interrupts
seta20.1:
	dec %cx                 # Timeout?
	jz seta20.3             # Yes
	inb $0x64,%al           # Get status
	testb $0x2,%al          # Busy?
	jnz seta20.1            # Yes
	movb $0xd1,%al          # Command: write
	outb %al,$0x64          #  output port
seta20.2:
	inb $0x64,%al           # Get status
	testb $0x2,%al          # Busy?
	jnz seta20.2            # Yes
	movb $0xdf,%al          # Command: enable
	outb %al,$0x60          #  A20
seta20.3:
	sti                     # Enable interrupts

### Finally, call the boot code we've successfully loaded.
final:
	jmp *%bx

err_pt:
	movw $msg_pt,%si        # "Invalid partition table"
	jmp putstr              # Error out
err_edd:
	movw $msg_edd,%si       # "EDD unavailable"
	jmp putstr              # Error out.
	

### Call the BIOS to read the disk. The number of sectors to read
### should be in %bx; the starting location should be in %si. This
### uses the registers %di, %ah, %bx, and %cx.
read:
	movw %sp,%di            # Save stack pointer
	pushl $0x0              # Set the LBA
	pushl (%si)             #  address
	pushw %es               # Set the address of
	pushw $LOAD             #  the transfer buffer
	pushw %bx               # Read n sectors
	pushw $0x10             # Packet length
	movw %sp,%si            # Packet pointer
	movw $0x4200,%ax        # BIOS: LBA read from disk
	int $0x13               # Call the BIOS
	movw %di,%sp            # Restore stack
	ret                     # Return

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

msg_pt: .asciz "Invalid partition table"
msg_edd:.asciz "EDD unavailable"

	.org PT_OFF,0x90        # Fill with NOP
partbl: .fill 0x10,0x4,0x0      # Partition table
	.word MAGIC             # Magic bootable number
