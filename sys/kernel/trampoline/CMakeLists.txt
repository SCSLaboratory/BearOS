set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -Wl,--oformat=binary,-Ttext=0x6000,-N,-S,-etrampoline_start")

# collect up all the source files
file (GLOB SRC_FILES trampoline.S)

# build the kernel executable from the sources
add_executable(tramp ${SRC_FILES})
