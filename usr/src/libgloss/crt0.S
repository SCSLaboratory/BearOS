.global _start
.extern main, exit, env_init

_start:
	
# before we enter main, we have to setup the pointer the the environment
# it may have been moved by diversity. 
# it's location however, is stored in register R10

movabs $0x0, %rbx
.reloc .-8, R_X86_64_64, env_init
callq *%rbx

# call the user's function. done with a relocation for diversity!
movabs $0x0,%rbx
.reloc .-8, R_X86_64_64, main
callq *%rbx

# don't mess with this, it was missing and is ABSOLUTELY necessary to return a value to exit (RMD)
movq %rax, %rdi
 
# call the 'kill me' syscall to end. again, done with a relocation
movabs $0x0,%rbx
.reloc .-8,R_X86_64_64, _exit
callq *%rbx
 
# loop in case we haven't yet rescheduled
lp:
hlt
jmp lp
