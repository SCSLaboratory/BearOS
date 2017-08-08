# collect up all the source files 
set(SRC_FILES 
  vgad.c
  vga_driver.c
  ${BEAR_SOURCE_DIR}/usr/sbin/pio/pio.S
)

# build the kernel executable from the sources
add_executable(vgad ${SRC_FILES})


# link the kernel with the libraries not using libnfs
target_link_libraries(vgad ${NEWLIB_LIBS} ${NEWLIB_LIBS})

