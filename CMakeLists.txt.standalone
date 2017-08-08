# CMakeLists.txt.standalone -- Used to build the system for the Dell Optiplex 
# without any networking or services. 
# This file is used by "mkall -s"

cmake_minimum_required (VERSION 2.8)
set(CMAKE_C_COMPILER "gcc")
project (BEAR)

set(DEFAULT_FLAGS "-ggdb -fno-builtin -ffreestanding -fno-stack-protector -mcmodel=large -nostdinc -nostdlib -MMD -DSTANDALONE")

set(SYSTEM_FLAGS "-DSERIAL_OUT_SYSTEM -DDIVERSITY -DENABLE_SMP -DXONLY_BEAR_HYPV")

# USER_FLAGS: NFS used in the system only for compilation -- not in default flags
set(USER_FLAGS "${DEFAULT_FLAGS} -DUSER -ffunction-sections -DBEARNFS")

# Set temporary GCC include directories
set(GCC_INCLUDE_DIR "/usr/lib/gcc/x86_64-linux-gnu/4.4.3/include")

# Enable verbose compiling
#set(CMAKE_VERBOSE_MAKEFILE on)

# Enable C-language and ASM
enable_language(C ASM)

# Endable testing
enable_testing()

# Set the output paths for libraries and executables
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# Set the global include directory
set(USER_INCLUDES ${BEAR_SOURCE_DIR}/usr/include
		  ${GCC_INCLUDE_DIR})

# Start adding the source tree to cmake's compilation process
# shared comes first as it is referenced by both, sys and usr
add_subdirectory(sys)
add_subdirectory(usr)
