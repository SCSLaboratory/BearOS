# Note that the system daemon may need to send messages to itself (e.g. printf)

# sourses used for the system daemon
set(SYSD_FILES sysd.c 
  sysd_utils.c
  ${BEAR_SOURCE_DIR}/usr/sbin/daemond/daemond_if.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/shash.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# sources used for the system daemon interface
set(SYSD_IF_FILES sysd_if.c sysd_utils.c)

set(SYSLIBS   
  ${BEAR_BINARY_DIR}/lib/newlib/libc.a
  ${BEAR_BINARY_DIR}/lib/newlib/libg.a
  ${BEAR_BINARY_DIR}/lib/newlib/libm.a
  ${BEAR_BINARY_DIR}/lib/libgloss.a
  ${BEAR_BINARY_DIR}/lib/libsyscall.a
)

# build the system daemon 
add_executable(sysd ${SYSD_FILES})

target_link_libraries(sysd ${SYSLIBS} libsyscall.a ${SYSLIBS} ${SYSLIBS})


# build the interface library -- used as part of NEWLIB to implement libgloss
add_library(sysd_if STATIC ${SYSD_IF_FILES})




