# set compiler flags
set(CMAKE_C_FLAGS ${USER_FLAGS})
set(CMAKE_ASM_FLAGS ${USER_FLAGS})
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -Wl,-N,-S,-q,-T -Wl,${BEAR_SOURCE_DIR}/tools/linker/userbin.ld")

include_directories(${USER_INCLUDES})

# these directories are in the build directory and are architecture specific
link_directories (
  ${BEAR_BINARY_DIR}/lib
  ${BEAR_BINARY_DIR}/lib/newlib
)

# collect up all the usr libraries - Must be in this order! 
# must be able to talk with system daemon
set(NEWLIB_LIBS
#  ${BEAR_BINARY_DIR}/lib/libsysd_if.a
  ${BEAR_BINARY_DIR}/lib/libsyscall.a
  ${BEAR_BINARY_DIR}/lib/libgloss.a
  ${BEAR_BINARY_DIR}/lib/newlib/libc.a
  ${BEAR_BINARY_DIR}/lib/newlib/libg.a
  ${BEAR_BINARY_DIR}/lib/newlib/libm.a
)
	

add_subdirectory(src)
add_subdirectory(sbin)
add_subdirectory(src/utils)
add_subdirectory(test)
