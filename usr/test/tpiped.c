/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/
/*
 * tpiped.c -- The piped regression test
 *
 * Each Daemon has an associated test.  Eventually, every daemon will
 * have its own log; for the time being, only enable printing from
 * either the test or the daemon, but not both. Each has its own DEBUG
 * flag to allow you to choose which ONE will do printing. They both
 * use the same printing routine for messages in piped_utils.c.
 */
#include <stdlib.h>		/* EXIT_FAILURE/EXIT_SUCCESS */
#include <stdio.h>		/* printf */
#include <unistd.h>
#include <syscall.h>		/* fork/exec */
#include <msg.h>		/* msgsend/msgrecv */
#include <string.h>		/* strsep */
#include <newlib.h>
#include <stdint.h>

#include <sys/wait.h>		/* waitpid */

#include <sbin/syspid.h>	/* defines the daemons id e.g. PIPED */
#include <sbin/piped.h>	/* interface: cf. piped_if.c & piped_utils.c */

#include <utils/tutils.h>	/* start_daemon */
#include <utils/bool.h>		/* TRUE/FALSE */

static int pipe_test(void);

/* 
 * tpiped -- Regression test for a piped.  Sends an INIT message,
 * some PING messages, and ends with a QUIT message.
 *
 */
int main(int argc, char *argv[]) {

  int filedes[2], pid;
  int i,status,dpid,drc;	/* daemon pid and return code */

  /* open a pipe */
  if ( pipe(filedes) != 0 ) {
    printf("failed to create pipe\n");
    return -1;
  }

  if ( pipe_test() ) {
    printf("PIPED Test Failure.\n");
    exit(EXIT_FAILURE);
  }

  close(filedes[0]);
  close(filedes[1]);

  /* responded correctly */
  exit(EXIT_SUCCESS);
}


static int pipe_test(void) {

  int pid, i, ret, j;
  int status, len;
  char *buf;
  int fd1[2], fd2[2];

  buf = (char*)malloc(PIPE_LENGTH / 2 * sizeof(char));
  if ( !buf ) {
    printf("can't malloc PIPE_LENGTH/2\n");
    return -1;
  }

  if ( pipe(fd1) ) {
    printf("Allocating the first pipe failed\n");
    return -1;
  }

  /* *************** FIRST TEST -> Write to full, then read ************ */

  pid = fork();

  if ( pid ) {
    
    waitpid(pid, &status, 0);

    if ( WEXITSTATUS(status) != 0 )
      return -1;
  }
  else {
    
    if ( pipe(fd2) ) {
      printf("Allocating the second pipe failed\n");
      exit(EXIT_FAILURE);
    }

    pid = fork();

    if ( pid ) { /* parent. wait, then read. */
      
      waitpid(pid, &status, 0);
      
      /* two rounds for whole pipe length */
      for ( j = 0; j < 2 ; j++ ) {
	
	/* read until you have as much as you want. */
	for ( len = 0; len < PIPE_LENGTH/2; len += ret ) {

	  ret = read(fd2[0], buf, (PIPE_LENGTH/2) - len);

	  if ( ret <= 0 ) {
	    printf("Read failed!\n");
	    exit(EXIT_FAILURE);
	  }
	}

	/* verify that we got the proper data */
	for ( i = 0; i < PIPE_LENGTH/2; i++ ) {
	  if ( buf[i] != 't' ) {
	    printf("Test 1: Unexpected data %c in the buffer %d\n", buf[i], i);
	    exit(EXIT_FAILURE);
	  }
	}

	/* reset the buffer */
	memset(buf, '\0', PIPE_LENGTH/2);
      }

      close(fd2[0]);

      exit(EXIT_SUCCESS);
    }
    else { /* child, write then close, exit */
  
      memset(buf, 't', PIPE_LENGTH/2);
      
      write(fd2[1], buf, PIPE_LENGTH/2);
      write(fd2[1], buf, PIPE_LENGTH/2);
      
      close(fd2[1]);

      exit(EXIT_SUCCESS);
    }
  }

  /* *********** SECOND TEST -> Write to full, then read, but first offset 
                   so that we nkow it will wrap the circular buffer ****** */

  pid = fork();

  if ( pid ) {
    
    waitpid(pid, &status, 0);

    if ( WEXITSTATUS(status) != 0 )
      return -1;
  }
  else {
    
    if ( pipe(fd2) ) {
      printf("Allocating the second pipe failed\n");
      exit(EXIT_FAILURE);
    }

    pid = fork();

    if ( pid ) { /* parent. wait, then read. */
      
      waitpid(pid, &status, 0);
      
      for ( j = 0; j < 2 ; j++ ) {

	/* read until you have as much as you want. */
	for ( len = 0; len < PIPE_LENGTH/2; len += ret ) {

	  ret = read(fd2[0], buf, (PIPE_LENGTH/2) - len);

	  if ( ret <= 0 ) {
	    printf("Read failed!\n");
	    exit(EXIT_FAILURE);
	  }
	}

	/* verify that we got the data we expected. */
	for ( i = 0; i < PIPE_LENGTH/2; i++ ) {
	  if ( buf[i] != 't' ) {
	    printf("Test 2: Unexpected data %c in the buffer %d\n", buf[i], i);
	    exit(EXIT_FAILURE);
	  }
	}

	/* reset the buffer */
	memset(buf, '\0', PIPE_LENGTH/2);
      }

      close(fd2[0]);
      
      exit(EXIT_SUCCESS);
    }
    else { /* child, write then close, exit */
      
      memset(buf, 't', PIPE_LENGTH/2);
      
      /* first create an offset in the buffer */
      write(fd2[1], buf, 100);
      read(fd2[0], buf, 100);
      
      /* now write to fill the buffer */
      write(fd2[1], buf, PIPE_LENGTH/2);
      write(fd2[1], buf, PIPE_LENGTH/2);

      close(fd2[1]);
      
      exit(EXIT_SUCCESS);
    }
  }

  /* *************** THIRD TEST - write and read at same time ************ */

  pid = fork();

  if ( pid ) {
    
    waitpid(pid, &status, 0);

    if ( WEXITSTATUS(status) != 0 )
      return -1;
  }
  else {
    
    if ( pipe(fd2) ) {
      printf("Allocating the second pipe failed\n");
      exit(EXIT_FAILURE);
    }

    pid = fork();
    
    if ( pid ) { /* parent. read. */
      
      for ( j = 0; j < 100; j++ ) {

	/* read until you have as much as you want. */
	for ( len = 0; len < 1024; len += ret ) {

	  ret = read(fd2[0], buf, 1024 - len);

	  if ( ret < 0 ) {
	    printf("Read failed!\n");
	    exit(EXIT_FAILURE);
	  }
	}

	
	for ( i = 0; i < 1024; i++ ) {
	  if ( buf[i] != 't' ) {
	    printf("Test 3: Unexpected data %c in the buffer %d\n", buf[i], i);
	    exit(EXIT_FAILURE);
	  }
	}	
      }

      close(fd2[0]);      
      waitpid(pid,&status,0); /*collect zombie child */
      exit(EXIT_SUCCESS);
    }
    else { /* child, write then close, exit */
  
      memset(buf, 't', PIPE_LENGTH/2);

      for ( i = 0; i < 100; i++ ) 
	write(fd2[1], buf, 1024);
      
      close(fd2[1]);
      
      exit(EXIT_SUCCESS);
    }
  }

  close(fd1[0]);
  close(fd1[1]);

  return 0;
}
