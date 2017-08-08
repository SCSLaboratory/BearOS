set(USER_FLAGS "${USER_FLAGS} -Wall")
set(CMAKE_C_FLAGS ${USER_FLAGS})
set(CMAKE_ASM_FLAGS ${USER_FLAGS})

# collect all the sources
file (GLOB SRC_FILES *.c *.S)

# add shared library to system 
add_library(syscall STATIC ${SRC_FILES})

