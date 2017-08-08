# Daemon source files
set(DAEMOND_FILES daemond.c daemond_utils.c)

# Daemon interface files
set(DAEMOND_IF_FILES daemond_if.c daemond_utils.c)

# Build the daemon
add_executable(daemond ${DAEMOND_FILES})

target_link_libraries(daemond ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})

# Build the daemon interface library
add_library(daemond_if STATIC ${DAEMOND_IF_FILES})
