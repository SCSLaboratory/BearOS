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

/* kstdio.c -- Kernel versions of useful C stdio functions.
 *
 */
#include <kstdarg.h>
#include <kstdio.h>
#include <stdint.h>
#include <fatfs.h>
#include <constants.h>
#include <pio.h>

#ifdef VGA_OUT_SYSTEM
#include <sbin/vgad.h>
#endif

#ifndef USER			/* ensure only used in system */

#define STDOUT NULL

#define TABSTOP 8

static int putc(int c, FIL *fp);
static void do_print(const char *fmt, va_list argp, FIL *stream);
static void fputs(const char *s, FIL *fp);

int kputchar(int c) {
  return putc(c,STDOUT);
}

void kputs(const char *s) {
  fputs(s,STDOUT);
  return;
}

#ifdef SERIAL_OUT_SYSTEM
#define PORT 0x3f8   /* COM1 */
int is_transmit_empty() {
   return inb(PORT + 5) & 0x20;
}

void write_serial(char a) {
   while (is_transmit_empty() == 0);
    outb(PORT,a);
}
#endif

/* Parts of the following functions were adapted from Minix stdio code.
 * See LICENSE.Minix in the top-level Bear directory for the licensing
 * information for those sections.
 */
void kprintf(const char *fmt, ...) {
  va_list argp;
  
  va_start(argp, fmt);
  
  do_print(fmt, argp, STDOUT);

  va_end(argp);

  return;
}

static int putc(int c, FIL *fp) {
  
  if (fp == STDOUT) {
    if((c) == '\n'){
#ifdef SERIAL_OUT_SYSTEM

      write_serial(0xA);
#endif	/* SERIAL_OUT_SYSTEM */
#ifdef VGA_OUT_SYSTEM
      vga_newline();
#endif	/* VGA_OUT_SYSTEM */
    }
    else if((c) == '\t') {
#ifdef SERIAL_OUT_SYSTEM

      write_serial(0x9);
#endif	/* SERIAL_OUT_SYSTEM */
#ifdef VGA_OUT_SYSTEM
      vga_tabline();
#endif	/* VGA_OUT_SYSTEM */
    }
    else {
#ifdef SERIAL_OUT_SYSTEM

	write_serial((char)c);

#endif	/* SERIAL_OUT_SYSTEM */
#ifdef VGA_OUT_SYSTEM
      vga_drawc((char)c, STD_VGA_COLOR);
      vga_advance(1);
#endif	/* VGA_OUT_SYSTEM */
    }
  }
  return c;
}

static void fputs(const char *s, FIL *fp) {
  char *p = (char *)s;
  int c;
  
  while((c = *p++) != '\0')
    putc(c,fp);

  putc('\n',fp);
  
  return;
}

static void do_print(const char *fmt, va_list argp, FIL *stream) {
  int c;                                        /* Next character in fmt */
  int d;
  uint64_t u;                                   /* Hold number argument */
  int base;                                     /* Base of number arg */
  int negative;                                 /* Print minus sign */
  static char x2c[] = "0123456789ABCDEF";       /* Number converstion table */
  char ascii[8*sizeof(long)/3 + 2];             /* String for ASCII number */
  char *s;                                      /* String to be printed */
  int min_len;
  int position;

  position=0;			/* position on line */
  while((c = *fmt++) != 0) {
    if(c == '%') {
      negative = 0;                         /* (Re)initialize */
      s = NULL;                             /* (Re)initialize */
      switch(c = *fmt++) {
	/* Known keys are %d, %u, %x, %s */
      case 'd':
	d = va_arg(argp, int32_t);
	if(d < 0) {
	  negative = 1;
	  u = -d;
	} else {
	  u = d;
	}
	base = 10;
	min_len = 0;
	break;
      case 'u':
	u = va_arg(argp, uint64_t);
	base = 10;
	min_len = 0;
	break;
      case 'x':
	u = va_arg(argp, uint64_t);
	base = 0x10;
	min_len = 2; /* ST: was 0 ?? */
	break;
      case 'X':
	u = va_arg(argp, uint64_t);
	base = 0x10;
	min_len = 2;
	break;
      case 's':
	s = va_arg(argp, char *);
	if(s == NULL)
	  s = "(null)";
	break;
      default:
	s = "%?";
	s[1] = c;
      }
      /* Assume a number if no string is set. Convert to ASCII. */
      if(s == NULL) {
	s = ascii + sizeof(ascii) - 1;
	*s = 0;
	do {
	  *--s = x2c[(u % base)];       /* work backwards */
	  min_len--;
	} while(((u /= base) > 0) || (min_len > 0));
      }
      /* This is where the actual output for format "%key" is done. */
      if(negative) {
	putc('-', stream);
	position++;
      }
      while(*s != '\0') {
	putc(*s++, stream);
	position++;
      }
    } 
    else {			/* not formatting an arg */
      if(c=='\t') {
	if((position%TABSTOP)==0) { /* on a tabstop move off it */
	  putc(' ',stream);
	  position++;
	}
	while((position%TABSTOP)!=0) { /* keep going till next */
	  putc(' ',stream);
	  position++;
	}
      }
      else {      /* Print and continue. */
	putc(c, stream);
	position++;
      }
    }
  }
}

#endif /* ifndef USER */
