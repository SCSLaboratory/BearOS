# collect up all the source files 
set(SRC_FILES kbd.c
  ${BEAR_SOURCE_DIR}/usr/sbin/pio/pio.S
)

# build the kernel executable from the sources
add_executable(kbd ${SRC_FILES})


# link the kernel with the libraries not using libnfs
target_link_libraries(kbd ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})

 
