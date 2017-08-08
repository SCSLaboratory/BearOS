set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -Wl,--oformat=binary,-Ttext=0x7c00,-N,-S,-estart")

# collect up all the source files
file (GLOB SRC_FILES *.S)

# build the kernel executable from the sources
add_executable(boot1 ${SRC_FILES})
