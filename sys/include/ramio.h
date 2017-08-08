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

#pragma once

#include <ff_types.h>
#include <stdint.h>

#define RAMDISK         0x5010   /* Variable for location of ramdisk in phys memory */
#define RAMDISK_SIZE    0xA00000

/* Status of Disk Functions */
typedef BYTE    DSTATUS;

typedef enum {
        RES_OK = 0,             /* 0: Successful */
        RES_ERROR,              /* 1: R/W Error */
        RES_WRPRT,              /* 2: Write Protected */
        RES_NOTRDY,             /* 3: Not Ready */
        RES_PARERR              /* 4: Invalid Parameter */
} DRESULT;

/* Read from the inital ram disk and put it at 
	void *read_buf location with 
	int size
 */
DRESULT ramdisk_read( BYTE drive, BYTE *read_buf, DWORD LBA, BYTE count);

DRESULT user_ramdisk_read(BYTE drive, BYTE *read_buf, DWORD LBA, BYTE count);

/* For compatibility resons -- Not used though */

DRESULT disk_ioctl( unsigned char drive, unsigned char command, void *buffer ); 

DRESULT disk_write(BYTE, const BYTE*, DWORD, BYTE);

DSTATUS disk_status(BYTE);

DSTATUS disk_init();

/* For compatibility reasons -- Not used though */

/* Disk Status Bits (DSTATUS) */
#define STA_NOINIT              0x01    /* Drive not initialized */
#define STA_NODISK              0x02    /* No medium in the drive */
#define STA_PROTECT             0x04    /* Write protected */


/* Command code for disk_ioctrl fucntion */

/* Generic command (mandatory for FatFs) */
#define CTRL_SYNC                       0       /* Flush disk cache (for write functions) */
#define GET_SECTOR_COUNT        1       /* Get media size (for only f_mkfs()) */
#define GET_SECTOR_SIZE         2       /* Get sector size (for multiple sector size (_MAX_SS >= 1024)) */
#define GET_BLOCK_SIZE          3       /* Get erase block size (for only f_mkfs()) */
