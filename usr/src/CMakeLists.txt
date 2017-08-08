# Build the CORE Libraries 
add_subdirectory(libsyscall)	# system call interface
add_subdirectory(libgloss)	# stdlib interface
add_subdirectory(liblwip)	# network stack interface
add_subdirectory(libsocket)	# socket interface
add_subdirectory(apps)	# application program written by users
