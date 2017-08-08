include_directories(${USER_INCLUDES}
	${BEAR_SOURCE_DIR}/usr/include/sbin/libnfs
	${BEAR_SOURCE_DIR}/usr/include/sbin/libnfs/nfsc
)

# Daemon source files 
set(NFSD_FILES nfsd.c
  ${BEAR_SOURCE_DIR}/usr/sbin/nfsd/nfsd_utils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/shash.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# Daemon interface files 
set(NFSD_IF_FILES nfsd_if.c nfsd_utils.c)

# Build the daemon
add_executable(nfsd ${NFSD_FILES})

target_link_libraries(nfsd ${NEWLIB_LIBS} libsocket.a libnfs.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})

# Build the deamon interface library
add_library(nfsd_if STATIC ${NFSD_IF_FILES})
