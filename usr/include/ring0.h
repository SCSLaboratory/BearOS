#ifndef _RING0_H_
#define _RING0_H_

#include <stdint.h>
#include <msg.h>

struct gdt_desc {
	uint16_t limit;
	uint64_t base;
} __attribute__ ((packed));

struct idt_desc {
	uint16_t limit;
	uint64_t base;
} __attribute__ ((packed));


void send_ipi(uint8_t apicid, uint32_t int_vector);

uint64_t lapicaddr;

int kernel_core_apicid;

int _proc_loaded_in_ring0;

/* for the circular syscall buffer */
Message_t *_sc_rbuf;
uint64_t _sc_rbuf_size;
uint64_t _sc_rbuf_head;
uint64_t _sc_rbuf_tail;

struct idt_desc _idtp;
struct gdt_desc _gdtp;

struct mcontext *_mcontext;

volatile uint64_t _next_proc_cr3;

uint64_t _first_time;

uint64_t _faulting_rip;

enum {
  APIC_APICID =  	0x20,
  APIC_APICVER =        0x30,
  APIC_TASKPRIOR =	0x80,
  APIC_EOI =		0x0B0,
  APIC_LDR =	 	0x0D0,
  APIC_DFR =	 	0x0E0,
  APIC_SPURIOUS =	0x0F0,
  APIC_ESR =	 	0x280,
  APIC_ICRL =	 	0x300,
  APIC_ICRH =	 	0x310,
  APIC_LVT_TMR =	0x320,    //lvtt
  APIC_LVT_PERF =	0x340,
  APIC_LVT_LINT0 =	0x350,
  APIC_LVT_LINT1 =	0x360,
  APIC_LVT_ERR =	0x370,
  APIC_TMRINITCNT =     0x380,		//tmict  
  APIC_TMRCURRCNT =	0x390,    //tmcct
  APIC_TMRDIV =	 	0x3E0,       //tdcr
  APIC_LAST =	 	0x38F,
  APIC_DISABLE =	0x10000,
  APIC_SW_ENABLE =	0x100,
  APIC_CPUFOCUS =	0x200,
  APIC_NMI =	 	0x0400,
  TMR_PERIODIC =	0x20000,
  TMR_BASEDIV =	 	0x100000,
  APIC_FIELD = 		0x00000000,   // No shorthand
  APIC_DEASSERT = 	0x00000000,  // Deassert level-sensitive interrupt
  APIC_ASSERT =		0x00004000,   // Assert level-sensitive interrupt
  APIC_ALLINC =		0x00080000,   // All including self
  APIC_LEVEL = 		0x00008000,
  APIC_INIT = 		0x00000500,  // INIT/RESET
  APIC_EDGE = 		0x00000000,  // Trigger Mode (RW)
  APIC_DELIVS = 	0x00001000,  // Delivery Status (RO)
  APIC_STARTUP =	0x00000600,  // Startup IPI
  APIC_TDIV_1 =         0xB,
  APIC_TDIV_2 =         0x0,
  APIC_TDIV_4 =         0x1,
  APIC_TDIV_8 =         0x2,
  APIC_TDIV_16 =        0x3,
  APIC_TDIV_32 =        0x8,
  APIC_TDIV_64 =        0x9,
  APIC_TDIV_128 =       0xA,
};

#endif
