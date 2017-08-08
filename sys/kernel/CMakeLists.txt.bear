# running hypervisor and kernel use next line and comment out following
set(KERNEL_FLAGS "${DEFAULT_FLAGS} ${SYSTEM_FLAGS} -DKERNEL -DBEAR_USERLAND_NET -ffunction-sections")
# running only kernel use next line
#set(KERNEL_FLAGS "${DEFAULT_FLAGS} ${SYSTEM_FLAGS} -ffunction-sections -DKERNEL -DBEAR_USERLAND_NET")
set(CMAKE_C_FLAGS ${KERNEL_FLAGS})
set(CMAKE_ASM_FLAGS ${KERNEL_FLAGS})

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -Wl,-q,-T -Wl,${BEAR_SOURCE_DIR}/sys/kernel/kernel.ld")

include_directories(${BEAR_SOURCE_DIR}/usr/include)

# collect up all the source files 
set(SRC_FILES 
  ${UTILS_DIR}/asm_subroutines.S
  ${UTILS_DIR}/pes.c
  ${UTILS_DIR}/kstdio.c
  ${UTILS_DIR}/kstring.c
  ${UTILS_DIR}/random.c
  ${UTILS_DIR}/kmalloc.c
  ${UTILS_DIR}/kqueue.c
  ${UTILS_DIR}/tsc.c
  ${UTILS_DIR}/pci.c
  ${UTILS_DIR}/asm_interrupts.S
  ${UTILS_DIR}/interrupts.c
  ${UTILS_DIR}/vmem_layer.c
  ${UTILS_DIR}/khash.c
  ${UTILS_DIR}/elf_loader.c
  ${UTILS_DIR}/ktime.c
  ${UTILS_DIR}/sha256.c
  ${UTILS_DIR}/local_apic.c
  ${UTILS_DIR}/ioapic.c
  ${UTILS_DIR}/smp.c
  ${UTILS_DIR}/semaphore.c
  ${UTILS_DIR}/ff.c
  ${UTILS_DIR}/file_abstraction.c
  ${UTILS_DIR}/ramio.c
  ${UTILS_DIR}/diversity.c
  ${UTILS_DIR}/list.c
  ${UTILS_DIR}/acpi.c
  ${UTILS_DIR}/vk.c
  ${UTILS_DIR}/vmx_utils.c
  ${USR_SRC_DIR}/libsyscall/swint.S
  ${USR_SBIN_DIR}/vgad/vga_driver.c
  ${USR_SRC_DIR}/libsyscall/msg.c
  kprocman.c
  ksyscall.c
  kernel.c
  ${UTILS_DIR}/ktimer.c
  kwait.c
  kmsg.c
  kvmem.c
  kvcall.c
  ksched.c
  kload.c
)

# build the kernel executable from the sources
add_executable(kernel ${SRC_FILES})

add_subdirectory(trampoline)