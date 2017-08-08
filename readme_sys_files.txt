sys:
  d boot
  f CMakeLists.txt
  d hypv
  d include
  d kernel
  d utils

sys/boot:
  d boot1
  d boot2
  f CMakeLists.txt
  d kboot2
  d mbr
  f README.Ccode

sys/boot/boot1: First stage bootloader brings us from boot to C code. It 
		cycles through the processesor's 16, then 32, finally 64 bit 
		modes as demanded by the x86 boot process.
  f boot1.S
  f CMakeLists.txt

sys/boot/boot2: Second stage bootloader initializes the system for hypervisor 
		or kernel launch starting from 64-bit mode
  f bootelf.c - Basic elf loading functionality for the bootloader to unpack 
                the hypv (or kernel) elf binary for loading. 
  f boot.ld - linker script for bootloader
  f bootmemory.c - Initialize virtual memory subsystem including the frame 
                   array. Also, map the ramdisk in virtual memory.
  f CMakeLists.txt
  f main.c - Entry point of stage 2 bootloader

sys/boot/kboot2: Second build instructions for the second stage bootloader. 
		 This instance will load the "kernel" binary rather than the 
		 "binaryte" binary. This is used when the hypervisor is loaded 
		 as binaryte, then it launches kboot to let the kernel boot.
  f CMakeLists.txt

sys/boot/mbr:
  f CMakeLists.txt
  f mbr.S

sys/hypv: hypervisor specific code and build instructions
  f asm.h
  f asm.S          - asm subroutines needed by the hypervisor specifically.
  f CMakeLists.txt
  f ept.c          - EPT memory management.
  f hypervisor.c   - entry point for the hypervisor.
  f hypv.ld        - hypervisor linker script.
  f vmexit.c       - handling routines for exits from the guest to the hypv.
  f vmx.c          - hardware vm initialization and control code.
  f vproc.c        - slightly more abstract vm management


./sys/include: Headers for all hypv and kernel sources.
  f acpi.h
  f apic.h
  f asmp.h
  f asm_subroutines.h
  f bootconstants.h
  f bootinterrupts.h
  f bootmemory.h
  f check_type.h
  f constants.h
  f container_of.h
  f disklabel.h
  f diversity.h
  f elf.h
  f elf_loader.h
  f ept.h
  f fatfs.h
  f ffconf.h
  f ff_const.h
  f ff_types.h
  f file_abstraction.h
  f hash.h
  f interrupts.h
  f ke1000.h
  f kernel.h
  f khash.h
  f kload.h
  f kmalloc.h
  f kmalloc-private.h
  f kmalloc_sites.h
  f kmsg.h
  f kqueue.h
  f ksched.h
  f kstdarg.h
  f kstdio.h
  f kstring.h
  f ksyscall.h
  f ktime.h
  f ktimer.h
  f kvcall.h
  f kvmem.h
  f kvmem_sites.h
  f kwait.h
  f list.h
  f mbr_partition.h
  f mcontext.h
  f memory.h
  f mregs.h
  f network.h
  f pci.h
  f pes.h
  f pio.h
  f proc.h
  f procman.h
  f ramio.h
  f random.h
  f semaphore.h
  f sha256.h
  f shadow_vmcs.h
  f sigcontext.h
  f smp.h
  f stdint.h
  f tsc.h
  f uuid.h
  f vector.h
  f vk.h
  f vkprivate.h
  f vmexit.h
  f vmx.h
  f vmx_utils.h
  f vproc.h

./sys/kernel:
  f CMakeLists.txt
  f CMakeLists.txt.bear
  d ke1000
  f kernel.c       - main kernel entry point and initialization routines
  f kernel.ld      - kernel linker script
  f kload.c        - functions for loading processes
  f kmsg.c         - kernel message passign system
  f kprocman.c     - process management routines
  f ksched.c       - scheduling routines
  f ksyscall.c     - functionality for processing different system calls
  f kvcall.c       - kernel's vmcall wrapper
  f kvmem.c        - some kernel-specific memory management tools 
                     including management for device memory
  f kwait.c        - process "wait" management as well as general process 
                     hierarchy and THE GRIM REAPER

./sys/kernel/ke1000:
  f ke1000.c     - networking stuff. Not used in the provided build, but the 
                   hypervisor has some networking logic outside of ifdefs... 
		   Consider uncoupling hypv from the network handling to be a 
		   TODO

./sys/utils:
  f acpi.c
  f asm_interrupts.S
  f asm_subroutines.S
  f CMakeLists.txt
  f diversity.c
  f elf_loader.c
  f ff.c
  f file_abstraction.c
  f interrupts.c
  f ioapic.c
  f khash.c
  f kmalloc.c
  f kqueue.c
  f kstdio.c
  f kstring.c
  f ktime.c
  f ktimer.c
  f list.c
  f local_apic.c
  f pci.c
  f pes.c
  f ramio.c
  f random.c
  f semaphore.c
  f sha256.c
  f smp.c
  f tsc.c
  f vector.c
  f vk.c
  f vmem_layer.c
  f vmx_utils.c
