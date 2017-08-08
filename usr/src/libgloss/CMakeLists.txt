set(CMAKE_C_FLAGS ${USER_FLAGS})
set(CMAKE_ASM_FLAGS ${USER_FLAGS})

# collect all the sources
file (GLOB SRC_FILES *.c *.S
  ${BEAR_SOURCE_DIR}/usr/sbin/nfsd/nfsd_if.c
  ${BEAR_SOURCE_DIR}/usr/sbin/piped/piped_if.c
  ${BEAR_SOURCE_DIR}/usr/sbin/net/netd/libsocket.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/hash.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# add shared library to system 
add_library(gloss STATIC ${SRC_FILES})

# crt.o
add_library(crt0 STATIC crt0.S)
SET_TARGET_PROPERTIES(crt0 PROPERTIES RULE_LAUNCH_LINK "${CMAKE_SOURCE_DIR}/cpo.sh ${CMAKE_BINARY_DIR}/lib <OBJECTS> --")
