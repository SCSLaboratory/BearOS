set(BOOT_FLAGS "${DEFAULT_FLAGS} ${SYSTEM_FLAGS} -DBOOTLOADER")

set(CMAKE_C_FLAGS ${BOOT_FLAGS})
set(CMAKE_ASM_FLAGS ${BOOT_FLAGS})

add_subdirectory(mbr)
add_subdirectory(boot1)
add_subdirectory(boot2)
add_subdirectory(kboot2)
