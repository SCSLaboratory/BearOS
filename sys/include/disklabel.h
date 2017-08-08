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

/* disklabel.h -- define the physical structure of a disk label.
 * 
 * This code is derived from software contributed to the DragonFly project,
 * under the BSD license. See LICENSE.BSD for more information on license
 * details. This code, while a derivative work, is not licensed under the BSD
 * license.
*/
#pragma once
#include <uuid.h>

/* Disklabels start at offset 0 on the slice they reside. All values are byte
 * offsets, not block numbers, in order to allow better portability. The 
 * on-disk format for a disklabel is slice-relative (all numbers are offsets 
 * relative to the slice on which they reside).
 *
 * Unlike the disklabel64 of Dragonfly BSD, the number of partitions is not
 * limited to 16 or 32; instead, it is variable.
 */
#define DISKMAGIC ((uint32_t)0xc4464c59)     /* the disk magic number */

/* A disklabel starts at slice relative offset 0, NOT SECTOR 1. In other words,
 * d_magic is at byte offset 512 within the slice, regardless of the sector
 * size.
 *
 * The d_reserved0 area is _not_ included in the crc and any kernel writeback 
 * of the label will not change the d_reserved area on-disk. It is purely a
 * mathematical shim to allow us to avoid sector calculations when reading or
 * writing the label. Since byte offsets are used in our disklabel, the entire
 * disklabel and the I/O required to access it becomes sector-agnostic.
*/
struct disklabel {
	uint8_t reserved0[512];          /* reserved/unused */
	uint32_t magic;                  /* the magic number */
	uint32_t crc;                    /* crc32() d_magic through end */
	uint32_t align;                  /* partition alignment requirement */
	uint32_t npartitions;            /* number of partitions */
	struct uuid stor_uuid;           /* unique uuid for label */

	uint64_t total_size;           /* total size of entire label (bytes) */
	uint64_t bbase;                /* boot area base offset (bytes) */
	                               /* boot area = pbase - bbase */
	uint64_t pbase;                /* first allocatable offset (bytes) */
	uint64_t pstop;                /* last allocatable offset+1 (bytes) */
	uint64_t abase;                /* location of backup copy if nonzero */

	uint8_t packname[64];
	uint8_t reserved1[64];

	/* Everything after this is the partition table, made up of struct partition. */
};

struct partition {                       /* An entry in the partition table */
	uint64_t boffset;                /* slice relative offset (bytes) */
	uint64_t bsize;                  /* size of partition (bytes) */
	uint8_t fstype;
	uint8_t unused[6];               /* reserved, must be 0 */
	struct uuid p_type_uuid;         /* mount type as UUID */
	struct uuid p_stor_uuid;         /* unique UUID for storage */
};
