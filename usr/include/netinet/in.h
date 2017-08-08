#pragma once

#include <stdint.h>

typedef uint16_t in_port_t;

typedef uint32_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};
