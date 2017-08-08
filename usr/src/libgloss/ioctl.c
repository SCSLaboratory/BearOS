

#ifdef TINYSSH

#include <stdio.h>
#include <sys/ioctl.h>

int ioctl(int fd, int req, ...) {

  /* dont die if changing the window size (but do nothing) */
  if(req==TIOCSWINSZ)
    return 0;
  else {
    printf("unsupported ioctl call\n");
    return -1;
  }
}

#endif 
