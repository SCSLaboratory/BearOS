set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -Wl,-T -Wl,${BEAR_SOURCE_DIR}/sys/boot/boot2/boot.ld,-S,-N")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DKBOOT")

set(BOOTSRC_FILES 
  ${UTILS_DIR}/kstring.c
  ${UTILS_DIR}/sha256.c
  ${UTILS_DIR}/random.c
  ${UTILS_DIR}/diversity.c
  ${UTILS_DIR}/kstdio.c
  ${UTILS_DIR}/interrupts.c
  ${UTILS_DIR}/asm_subroutines.S
  ../boot2/main.c
  ${UTILS_DIR}/vmem_layer.c
  ${UTILS_DIR}/kmalloc.c
  ${USR_SBIN_DIR}/vgad/vga_driver.c
  ${UTILS_DIR}/ramio.c
  ${UTILS_DIR}/ff.c
  ${UTILS_DIR}/elf_loader.c
  ${UTILS_DIR}/kqueue.c
  ${UTILS_DIR}/file_abstraction.c
  ../boot2/bootmemory.c
)

add_executable(kboot2 ${BOOTSRC_FILES})

