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
/* vga.c -- Small VGA driver for booting and reporting errors.
 *
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at bear/LICENSE.OpenSolaris
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at bear/LICENSE.OpenSolaris.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
*/

/*
 * Portions Copyright 2011 Morgon Kanter. 
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
*/

#include <stdint.h>
#include <syscall.h>
#include <msg.h>
#include <sbin/syspid.h>
#include <sbin/vgad.h>
#include <sbin/pio.h>
#include <sbin/vgareg.h>

#define VGA_COLOR_CRTC_INDEX 0x3d4
#define VGA_COLOR_CRTC_DATA 0x3d5

static unsigned short *vga_screen = (unsigned short *)0xb8000;

static void vga_set_crtc(int index, unsigned char val);
static unsigned char vga_get_crtc(int index);
static void vga_getpos(int*,int*);

void vga_set_loc(unsigned short *vs) {
	vga_screen = vs;
}

void vga_advance(int cols) {
	int row, col;

	vga_getpos(&row, &col);
	col += cols;
	while(col >= VGA_TEXT_COLS) {
		row += 1;
		col -= VGA_TEXT_COLS;
	}
	while(row >= VGA_TEXT_ROWS) {
		vga_scroll(3);
		row--;
		col = 0;
	}

	vga_setpos(row, col);
}

void vga_retreat(int cols) {
	int row, col;
	
	vga_getpos(&row, &col);
	col -= cols;
	while (col < 0) {
		row -= 1;
		col += VGA_TEXT_COLS;
	}
	if (row < 0) {
		row = 0;
		col = 0;
	}
	
	vga_setpos(row,col);
}

void vga_cursor_display(void) {
    unsigned char val, msl;

    /* Figure out the maximum scan line value. We need this to set the
     * cursor size. */
    msl = vga_get_crtc(VGA_CRTC_MAX_S_LN) & 0x1F;

    /* Enable the cursor and set its size. Preserve the upper two
     * bits of the control register.
     * - Bits 0-4 are the starting scan line of the cursor.
     *   Scanning is done from top-to-bottom. The top-most scan
     *   line is 0 and the bottom most scan line is the maximum scan
     *   line value.
     * - Bit 5 is the cursor disable bit.
    */
    val = vga_get_crtc(VGA_CRTC_CSSL);
    vga_set_crtc(VGA_CRTC_CSSL, (val & 0xC) | ((msl - 2) & 0x1F));

    /* Continue setting the cursor's size.
     * - Bits 0-4 are the ending scan line of the cursor.
     *   Scanning is done from top-to-bottom. The top-most scan
     *   line is 0 and the bottom most scan line is that maximum scan
     *   line value.
     * - Bits 5-6 are the cursor skew.
     */
    vga_set_crtc(VGA_CRTC_CESL, msl);
}

void vga_clear(int color) {
    uint16_t val;
    int i;

    val = (color << 8) | ' ';

    for(i = 0; i < VGA_TEXT_ROWS * VGA_TEXT_COLS; i++) {
        vga_screen[i] = val;
    }
}

void vga_drawc(int c, int color) {
    int row;
    int col;

    vga_getpos(&row, &col);
    vga_screen[row*VGA_TEXT_COLS + col] = (color << 8) | c;
}

void vga_newline() {
	int row;
	int col;

	vga_getpos(&row, &col);
	vga_advance(VGA_TEXT_COLS - col);
}

void vga_tabline() {
	int row;
	int col;

	vga_getpos(&row, &col);
	vga_advance((8-(col%8)));
}

void vga_scroll(int color) {
    unsigned short val;
    int i;

    val = (color << 8) | ' ';

    for(i = 0; i < (VGA_TEXT_ROWS-1) * VGA_TEXT_COLS; i++)
        vga_screen[i] = vga_screen[i + VGA_TEXT_COLS];
    for(; i < VGA_TEXT_ROWS * VGA_TEXT_COLS; i++)
        vga_screen[i] = val;
}

void vga_setpos(int row, int col) {
    int off;

    off = row * VGA_TEXT_COLS + col;
    vga_set_crtc(VGA_CRTC_CLAH, off >> 8);
    vga_set_crtc(VGA_CRTC_CLAL, off & 0xFF);
}

static void vga_getpos(int *row, int *col) {
    int off;

    off = (vga_get_crtc(VGA_CRTC_CLAH) << 8) + vga_get_crtc(VGA_CRTC_CLAL);
    *row = off / VGA_TEXT_COLS;
    *col = off % VGA_TEXT_COLS;
}

static void vga_set_crtc(int index, unsigned char val) {
    outb(VGA_COLOR_CRTC_INDEX, index);
    outb(VGA_COLOR_CRTC_DATA, val);
}

static unsigned char vga_get_crtc(int index) {
    outb(VGA_COLOR_CRTC_INDEX, index);
    return (inb(VGA_COLOR_CRTC_DATA));
}
