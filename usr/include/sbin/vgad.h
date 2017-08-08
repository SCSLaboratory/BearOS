#pragma once
/* vga.h -- header file for basic VGA driver.
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

/* These are serviced by the VGA device driver. */
/**** DEFINITIONS ****/
/* Incoming Message Types */
#define VGA_DRAWC   0
#define VGA_RETREAT 1
#define VGA_PUTS    2

/*
 * Interface to the bootstrap's internal VGA driver.
 */

#define	VGA_IO_WMR	0x3C8 /* vga io DAC write mode register */
#define	VGA_IO_DR	0x3C9 /* vga io DAC data register */
#define	VGA_IO_IS	0x3DA /* vga io input status register */

#define	VGA_TEXT_COLS		80
#define	VGA_TEXT_ROWS		25

#define STD_VGA_COLOR   3              /* Standard color for VGA printing     */

#define CHARBUF_SZ 1024

/* Message types associated with the VGA daemon */

/* syscall.h supplies: vgamem_req_t and vgamem_resp_t */

/* vga_backup */
typedef struct {
        int type;
        int cols;
} Vgaback_req_t;

typedef struct {
        int type;
        int ret;
} Vgaback_resp_t;

/* vga_backup */
typedef struct {
        int type;
        int cols;
} Retreat_req_t;

typedef struct {
        int type;
        int ret;
} Retreat_resp_t;

/* putchar */
typedef struct {
        int type;
        char c;
} Drawc_req_t;

typedef struct {
        int type;
        int ret;
} Drawc_resp_t;

typedef struct {
        int type;
        uint32_t len;
        char buf[CHARBUF_SZ];
} Puts_req_t;

typedef struct {
        int type;
        int ret;
} Puts_resp_t;

/* This tells us how big a buffer we need to recv any valid msg. */
typedef union {
  Vgaback_req_t vgaback_req;
  Vgaback_resp_t vgaback_resp;
  Drawc_req_t drawc_req;
  Drawc_resp_t drawc_resp;
  Retreat_req_t retreat_req;
  Retreat_resp_t retreat_resp;
  Puts_req_t puts_req;
  Puts_resp_t puts_resp;
} Vgad_msg_t;

void vga_set_loc(unsigned short*);
void vga_setpos(int, int);
void vga_clear(int);
void vga_scroll(int);
void vga_drawc(int, int);
void vga_cursor_display(void);
void vga_advance(int);
void vga_retreat(int);
void vga_newline(void);
void vga_tabline(void);

