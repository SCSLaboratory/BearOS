# Daemon source files
set(STATD_FILES statd.c statd_utils.c)

# Daemon interface files
set(STATD_IF_FILES statd_if.c statd_utils.c)

# Build the daemon
add_executable(statd ${STATD_FILES})

target_link_libraries(statd ${NEWLIB_LIBS} ${NEWLIB_LIBS})

# Build the daemon interface library
add_library(statd_if STATIC ${STATD_IF_FILES})
