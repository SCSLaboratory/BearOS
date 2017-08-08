# CMakeLists.txt.hw -- used for building the system for Dell Optiplex hardware.
# This file is used by "mkall -hw".

cmake_minimum_required (VERSION 2.8)
set(CMAKE_C_COMPILER "gcc")
project (BEAR)

# FLAGS: 
#    DEFAULT_FLAGS are general flags applied to all compilation
#    SYSTEM_FLAGS are flags specific to the hypervisor and kernel (bear/sys)
#    USER_FLAGS are flags specfic to user land (bear/usr)

# DEFAULT_FLAGS:
#   Additional Available Flags: 
#     KERNEL_DEBUG      -- kernel print from user land (kprintint/kprintstr)
#     REMOTE            -- Remote shell, no keyboard or vga
#     MINIMAL           -- Remote shell, no NFS
#     STANDALONE: Standalone system with no network daemons -- useful for removing impact 
#       of the network; NOTE: the STANDALONE flag is used in CMakeLists.txt.standalone 
#       which is used by "mkall -s". A standalone kernel with no network daemons can be
#       build with CMakeLists.txt.kernel which is used by "mkall -k".
#     BEARNFS           -- NFS is in use
set(DEFAULT_FLAGS "-ggdb -fno-builtin -ffreestanding -fno-stack-protector -mcmodel=large -nostdinc -nostdlib -MMD -DBEARNFS")

# SYSTEM_FLAGS:
#   Additional Available Flags: 
#     SERIAL_OUT_SYSTEM -- kernel prints to serial out
#     KERNEL_REFRESH    -- automatic kernel tear down (DO NOT USE) -- use test: trefresh 
#     DIVERSITY         -- adds diversity (-vm only)
#     ENABLE_SMP        -- adds multicore (-k only)
#     ENABLE_VTD        -- adds VTD support (Dell 9020 ONLY!)
#     VTD_HACK 		-- adds hack-ish support for VTD (9010 and blades)
#     VGA_OUT_SYSTEM    -- kernel prints to console
#     KMALLOC_TRACKING  -- track system memory (useful for detecting system memory leaks)
#     KVMEM_TRACKING    -- track virtual memory (useful for detecting user memory leaks)
#     EXCEPTION_DEBUG   -- provides RIP associated with faults
#     DEBUG             -- adds copious debug info
#     DEBUG_SMP         -- adds copious debug info for SMP
#     DELL_9020_KBD_FIX -- fixes BIOS issues associated with 9020 keyboard
#
# The next line is the usual default system setting
set(SYSTEM_FLAGS "-DSYSTEM -DSERIAL_OUT_SYSTEM  -DVTD_HACK -DDIVERSITY -DEXCEPTION_DEBUG -DXONLY_BEAR_HYPV")

# USER_FLAGS:
#   Additional Available Flags: 
#     SERIAL_OUT_USER   -- prints to serial out
set(USER_FLAGS "${DEFAULT_FLAGS} -DUSER -ffunction-sections")

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

# Set the global include directories for user code
set(USER_INCLUDES ${BEAR_SOURCE_DIR}/usr/include
		  ${GCC_INCLUDE_DIR})

set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# Start adding the source tree to cmake's compilation process
# shared comes first as it is referenced by both, sys and usr
add_subdirectory(sys)
add_subdirectory(usr)
