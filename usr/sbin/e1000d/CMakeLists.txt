# collect up all the source files 
set(SRC_FILES e1000d.c
	e1000d_sys.c
	${BEAR_SOURCE_DIR}/usr/sbin/pio/pio.S
	${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# build the kernel executable from the sources
add_executable(e1000d ${SRC_FILES})

# link the kernel with the libraries not using libnfs
target_link_libraries(e1000d ${NEWLIB_LIBS} ${NEWLIB_LIBS})

