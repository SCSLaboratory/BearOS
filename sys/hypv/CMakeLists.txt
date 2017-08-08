set(HYPV_FLAGS "${DEFAULT_FLAGS} ${SYSTEM_FLAGS} -ffunction-sections -DHYPV")
set(CMAKE_C_FLAGS ${HYPV_FLAGS})
set(CMAKE_ASM_FLAGS ${HYPV_FLAGS})

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -Wl,-q,-T -Wl,${BEAR_SOURCE_DIR}/sys/hypv/hypv.ld")

# collect up all the source files 
set(SRC_FILES 
  ${UTILS_DIR}/asm_subroutines.S
  ${UTILS_DIR}/pes.c
  ${UTILS_DIR}/kstdio.c
  ${UTILS_DIR}/kstring.c
  ${UTILS_DIR}/kmalloc.c
  ${UTILS_DIR}/kqueue.c
  ${UTILS_DIR}/khash.c
  ${UTILS_DIR}/asm_interrupts.S
  ${UTILS_DIR}/interrupts.c
  ${UTILS_DIR}/vmem_layer.c
  ${UTILS_DIR}/elf_loader.c
  ${UTILS_DIR}/pci.c
  ${UTILS_DIR}/tsc.c
  ${UTILS_DIR}/file_abstraction.c
  ${UTILS_DIR}/ff.c
  ${UTILS_DIR}/ramio.c
  ${UTILS_DIR}/sha256.c
  ${USR_SBIN_DIR}/vgad/vga_driver.c
  ${UTILS_DIR}/ktimer.c
  ${UTILS_DIR}/ktime.c
  ${UTILS_DIR}/random.c
  ${UTILS_DIR}/vector.c
  ${UTILS_DIR}/acpi.c
  ${UTILS_DIR}/ioapic.c
  ${UTILS_DIR}/vk.c
  ${UTILS_DIR}/vmx_utils.c
  asm.S
  vmexit.c
  vmx.c
  vproc.c
  hypervisor.c
  ept.c
  ${UTILS_DIR}/local_apic.c
  ${UTILS_DIR}/smp.c
  ${UTILS_DIR}/semaphore.c
  ../kernel/kvmem.c
)


# build the kernel executable from the sources
add_executable(hypv ${SRC_FILES})
