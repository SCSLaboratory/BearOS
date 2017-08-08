/*
 * DERIVED FROM Dropbear - a SSH2 server
 *
 * Copied from OpenSSH-3.5p1 source, modified by Matt Johnston 2003
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Allocating a pseudo-terminal, and making it the controlling tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/*RCSID("OpenBSD: sshpty.c,v 1.7 2002/06/24 17:57:20 deraadt Exp ");*/


#ifdef TINYSSH

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>		/* EXIT_FAILURE */
#include <termios.h>
#include <pty.h>		/* bear pty.h  */
#include <string.h>

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

/*
 * Allocates and opens a pty.  Returns 0 if no pty could be allocated, or
 * nonzero if a pty was successfully allocated.  On success, open file
 * descriptors for the pty and tty sides and the name of the tty side are
 * returned (the buffer must be able to hold at least 64 characters).
 */
int pty_allocate(int *ptyfd, int *ttyfd, char *namebuf, int namebuflen)
{
  /* BSD-style pty code. */
  char buf[64];
  int i;
  const char *ptymajors = "pqrstuvwxyzabcdefghijklmnoABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *ptyminors = "0123456789abcdef";
  int num_minors = strlen(ptyminors);
  int num_ptys = strlen(ptymajors) * num_minors;
  struct termios tio;

  for (i = 0; i < num_ptys; i++) {
    snprintf(buf, sizeof buf, "/dev/pty%c%c", ptymajors[i / num_minors],
	     ptyminors[i % num_minors]);
    snprintf(namebuf, namebuflen, "/dev/tty%c%c",
	     ptymajors[i / num_minors], ptyminors[i % num_minors]);
    
    *ptyfd = open(buf, O_RDWR | O_NOCTTY);
    if (*ptyfd < 0) {
      /* Try SCO style naming */
      snprintf(buf, sizeof buf, "/dev/ptyp%d", i);
      snprintf(namebuf, namebuflen, "/dev/ttyp%d", i);
      *ptyfd = open(buf, O_RDWR | O_NOCTTY);
      if (*ptyfd < 0) {
	continue;
      }
    }
    
    /* Open the slave side. */
    *ttyfd = open(namebuf, O_RDWR | O_NOCTTY);
    if (*ttyfd < 0) {
      fprintf(stdout,
		   "pty_allocate: %.100s: %.100s", namebuf, strerror(errno));
      close(*ptyfd);
      return 0;
    }
    /* set tty modes to a sane state for broken clients */
    if (tcgetattr(*ptyfd, &tio) < 0) {
      fprintf(stdout,
		   "ptyallocate: tty modes failed: %.100s", strerror(errno));
    } else {
      tio.c_lflag |= (ECHO | ISIG | ICANON);
      tio.c_oflag |= (OPOST | ONLCR);
      tio.c_iflag |= ICRNL;
      
      /* Set the new modes for the terminal. */
      if (tcsetattr(*ptyfd, TCSANOW, &tio) < 0) {
	fprintf(stdout,
		     "Setting tty modes for pty failed: %.100s",
		     strerror(errno));
      }
    }
    return 1;
  }
  fprintf(stdout, "Failed to open any /dev/pty?? devices");
  return 0;
}

/* Releases the tty.  Its ownership is returned to root, and permissions to 0666. */

void pty_release(const char *tty_name)
{
  if (chown(tty_name, (uid_t) 0, (gid_t) 0) < 0 && (errno != ENOENT)) {
    fprintf(stdout,"chown %.100s 0 0 failed: %.100s", tty_name, strerror(errno));
  }
  if (chmod(tty_name, (mode_t) 0666) < 0 && (errno != ENOENT)) {
    fprintf(stdout,"chmod %.100s 0666 failed: %.100s", tty_name, strerror(errno));
  }
}

/* Makes the tty the processes controlling tty and sets it to sane modes. */

void pty_make_controlling_tty(int *ttyfd, const char *tty_name)
{
#if 0
  int fd;

  /* First disconnect from the old controlling tty. */

#ifdef TIOCNOTTY
  fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
  if (fd >= 0) {
    (void) ioctl(fd, TIOCNOTTY, NULL);
    close(fd);
  }
#endif /* TIOCNOTTY */
  if (setsid() < 0) {
    fprintf(stdout,
		 "setsid: %.100s", strerror(errno));
  }

  /*
   * Verify that we are successfully disconnected from the controlling
   * tty.
   */
  fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
  if (fd >= 0) {
    fprintf(stdout,
		 "Failed to disconnect from controlling tty.\n");
    close(fd);
  }

  /* Make it our controlling tty. */
#ifdef TIOCSCTTY
  if (ioctl(*ttyfd, TIOCSCTTY, NULL) < 0) {
    fprintf(stdout,
		 "ioctl(TIOCSCTTY): %.100s", strerror(errno));
  }
#endif /* TIOCSCTTY */
  fd = open(tty_name, O_RDWR);
  if (fd < 0) {
    fprintf(stdout,
		 "%.100s: %.100s", tty_name, strerror(errno));
  } 
  else {
    close(fd);
  }

  /* Verify that we now have a controlling tty. */
  fd = open(_PATH_TTY, O_WRONLY);
  if (fd < 0) {
    fprintf(stdout,
		 "open /dev/tty failed - could not set controlling tty: %.100s",
		 strerror(errno));
  } 
  else {
    close(fd);
  }
#endif	/* 0 */
}

/* Changes the window size associated with the pty. */

void pty_change_window_size(int ptyfd, int row, int col,int xpixel, int ypixel)
{
	struct winsize w;
	w.ws_row = row;
	w.ws_col = col;
	w.ws_xpixel = xpixel;
	w.ws_ypixel = ypixel;
	(void) ioctl(ptyfd, TIOCSWINSZ, (void*)&w);
}

void pty_setowner(struct passwd *pw, const char *tty_name)
{
#if 0
	struct group *grp;
	gid_t gid;
	mode_t mode;
	struct stat st;

	/* Determine the group to make the owner of the tty. */
	grp = getgrnam("tty");
	if (grp) {
		gid = grp->gr_gid;
		mode = S_IRUSR | S_IWUSR | S_IWGRP;
	} else {
		gid = pw->pw_gid;
		mode = S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH;
	}

	/*
	 * Change owner and mode of the tty as required.
	 * Warn but continue if filesystem is read-only and the uids match/
	 * tty is owned by root.
	 */
	if (stat(tty_name, &st)) {
		printf("pty_setowner: stat(%.101s) failed: %.100s",
				tty_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (st.st_uid != pw->pw_uid || st.st_gid != gid) {
		if (chown(tty_name, pw->pw_uid, gid) < 0) {
			if (errno == EROFS &&
			    (st.st_uid == pw->pw_uid || st.st_uid == 0)) {
				fprintf(stdout,
					"chown(%.100s, %u, %u) failed: %.100s",
						tty_name, (unsigned int)pw->pw_uid, (unsigned int)gid,
						strerror(errno));
			} else {
				printf("chown(%.100s, %u, %u) failed: %.100s",
				    tty_name, (unsigned int)pw->pw_uid, (unsigned int)gid,
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}

	if ((st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO)) != mode) {
		if (chmod(tty_name, mode) < 0) {
			if (errno == EROFS &&
			    (st.st_mode & (S_IRGRP | S_IROTH)) == 0) {
				fprintf(stdout,
					"chmod(%.100s, 0%o) failed: %.100s",
					tty_name, mode, strerror(errno));
			} else {
				printf("chmod(%.100s, 0%o) failed: %.100s",
				    tty_name, mode, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}
#endif
}

#endif	/* TINYSSH */
