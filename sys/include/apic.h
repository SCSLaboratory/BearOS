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

#include <stdint.h>

/* See Table 1 of [1] */
#define IOAPIC_BASE		0xFEC00000

/* See Table 2 of [1] */
#define IOAPICID		0x00
#define	IOAPICVER		0x01
#define	IOAPICARB		0x02
#define	IOREDTBL		0x10
#define	REDTBL_ENTRY_LOW(irq) (2*irq)
#define	REDTBL_ENTRY_HIGH(irq) ((2*irq)+1)

#define INT_DISABLED    0x00010000  // Interrupt disabled
#define INT_LEVEL       0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW   0x00002000  // Active low (vs high)
#define INT_LOGICAL     0x00000800  // Destination is CPU id (vs APIC ID)
#define INT_REMOTE_IRR  0x00004000  // remote IRR

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

/* get ioapic address from mptable */
#define IOAPIC  0xFEC00000   // Default physical address of IO APIC

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.  
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.


uint64_t lapicaddr;
uint64_t ioapicaddr;

/* Interrupt vector for joining cores to guest */
#define HYPV_PSEUDO_SIPI 0x8C
/* Interrupt vector to start a vproc on a different core */
#define HYPV_START_VP 0x8D

/*Some APIC masks for the hypervisor*/
#define APIC_FIELD_MASK 0xfff
#define INIT_SIPI_MASK  0xf00

/*All of the fields in the MMIO APIC*/
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


/* Interrupts */
#define IRQ_OFFSET 32           /* Interrupts are mapped starting at trap 32 */
/*
  typedef enum {
  IRQ_TIMER =             0,
  IRQ_KEYBOARD =          1,
  IRQ_CASCADE =           2,
  IRQ_COM2 =              3,
  IRQ_COM1 =              4,
  IRQ_FLOPPY =            6,
  IRQ_LPT =               7,
  IRQ_RTC =               8,
  IRQ_PRI_IDE =           14,
  IRQ_SEC_IDE =       	15,
  IRQ_ERROR =         	19,
  IRQ_SPURIOUS =      	31,
  };*/


void lapic_init();
void ioapic_init();
void ioapicenable(uint32_t irq, uint32_t cpunum);
void ioapicdisable(uint32_t irq, uint32_t cpunum);
void ioapicwrite(uint32_t reg, uint64_t data);
uint64_t cpuGetAPICBase();
uint32_t this_cpu();
void lapic_start_aps(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
int calibrate_apic_timer(void);

uint32_t lapic_read(uint32_t offset);
void lapic_write(uint32_t offset, uint32_t data);
void send_ipi(uint8_t apicid, uint32_t int_vector);
