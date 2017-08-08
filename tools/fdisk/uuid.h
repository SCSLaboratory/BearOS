#pragma once
/* uuid.h -- universal unique identifier, DCE 1.1 compatible
 *
 * See:
 *     http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *     http://www.opengroup.org/onlinepubs/009629399/apdxa.htm
 *
 * Copyright (c) 2011 Bear System. All rights reserved.
 *
 * This code is derived from software contributed to the DragonFly project,
 * under the BSD license. See LICENSE.BSD for more information on license
 * details. This code, while a derivative work, is not licensed under the BSD
 * license.
*/

#include <stdint.h>

struct uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_hi_and_reserved;
	uint8_t clock_seq_low;
	uint8_t node[6];
} __attribute__ ((packed));
