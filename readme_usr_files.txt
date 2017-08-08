The userspace portion of the bear distribution is not intended to be the main 
  contribution, but is intented only to exercise the system half. With that in 
  mind, this file contains a high level description of what can be found in 
  userspace.

Note: A networked file system is not being included in this distribution, even 
  though it is supported by the bear system using NFS. As such, the source may 
  contain traces of the NFS system... these were left to aid anyone who 
  may wish to add this support in the future. Most notably missing is 
  usr/src/libnfs, which would contain a third party nfs library in a 
  system supporting nfs. 

Second note: Userspace files will discuss signal handling and job control. 
  Signal handling was not included or tested when multicore was added to the 
  system, and therefore these features are not currently functional.

usr/:
  d include - header files
  d sbin    - system daemons
  d src     - system libraries
  d test    - test programs

usr/sbin:
  d daemond - generic daemon code used to spawn the outline of a new daemon
  d e1000d  - network card daemon
  d kbd     - keyboard
  d netd    - network
  d nfsd    - network file system (not buildable) 
  d pio     - port io functions
  d piped   - pipes
  f README  - describes using daemond to make a new daemon
  d statd   - pings NFS to avoid sleep (not buildable) 
  d sysd    - system daemon - brings up the system
  d vgad    - VGA output daemon

usr/src:
  d apps       - place for userspace applications to go
  d libgloss   - glue for some functions "glossed over" by newlib
  d liblwip    - independently licensed network stack 
  d libsocket  - socket implementation
  d libsyscall - syscall backend - msg sending etc.
  d newlib     - independently licensed C library
  d utils      - slash (shell), some command line utilities
