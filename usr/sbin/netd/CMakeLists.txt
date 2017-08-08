
# collect up all the source files 
set(SRC_FILES bsock.c
	driver_api.c
	netd.c
	static_ip_table.c
        ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
        ${BEAR_SOURCE_DIR}/usr/src/utils/shash.c
)

# build the kernel executable from the sources
add_executable(netd ${SRC_FILES})

# link -- uses syscall to communcate with other daemons and lwip for a network stack
target_link_libraries(netd ${NEWLIB_LIBS} liblwip.a ${NEWLIB_LIBS})
