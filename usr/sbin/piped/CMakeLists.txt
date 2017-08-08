# Daemon source files
set(PIPED_FILES piped.c piped_utils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/lock.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/hash.c
)

# Daemon interface files
set(PIPED_IF_FILES piped_if.c piped_utils.c)

# Build the daemon
add_executable(piped ${PIPED_FILES})

target_link_libraries(piped ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})

# Build the daemon interface library
add_library(piped_if STATIC ${PIPED_IF_FILES})
