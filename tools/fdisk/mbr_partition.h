#pragma once
/* mbrtable.h -- representation of the on-disk MBR partition table.
 *
 * Copyright (c) 2011 The Bear System. All rights reserved.
*/

#include <stdint.h>

struct mbr_partition {
	uint8_t status;                          /* 0x80 = bootable, 0x0 = not */
	uint8_t chs_first[3];                    /* Addr of first sector */
	uint8_t ptype;                           /* Partition type */
	uint8_t chs_last[3];                     /* Addr of last sector */
	uint32_t lba;                            /* LBA of first sector */
	uint32_t numsectors;
};
