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

/*based on the Plan9 kernel*/

/* References:
 *   [1] http://www.intel.com/design/chipsets/datashts/29056601.pdf
 */

#include <pio.h>
#include <apic.h>
#include <smp.h>
#include <kstring.h>



#define IOAPIC  0xFEC00000   // Default physical address of IO APIC

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.  
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.

volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint32_t reg;
  uint32_t pad[3];
  uint32_t data;
};	

static uint32_t ioapicread(uint32_t reg)
{
    ioapic->reg = reg;
    return ioapic->data;
}

void ioapicwrite(uint32_t reg, uint64_t data)
{
    ioapic->reg = reg;
    ioapic->data = data;
}

void ioapicdisable(uint32_t irq, uint32_t cpunum)
{

    ioapicwrite(REG_TABLE+2*irq, INT_DISABLED | (IRQ_OFFSET + irq));
    ioapicwrite(REG_TABLE+2*irq+1, cpunum<<24);
}

  void
    ioapicenable(uint32_t irq, uint32_t cpunum)
  {
    // Mark interrupt edge-triggered, active high,
    // enabled, and routed to the given cpunum,
    // which happens to be that cpu's APIC ID.
    if(irq == 1)  
      ioapicwrite(REG_TABLE+2*irq, IRQ_OFFSET + irq );
    else
      ioapicwrite(REG_TABLE+2*irq,
		  (INT_LEVEL | IRQ_OFFSET) + irq );	
  
  
    ioapicwrite(REG_TABLE+2*irq+1, cpunum << 24);
  
  
  }


void ioapic_init(void)
{
    uint32_t i, id, maxintr;

    /*FIXME This only programs the first found ioapic*/
    ioapic = (volatile struct ioapic*)ioapicaddr;
    maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;


#ifdef DEBUG    
    kprintf("IOAPIC ID %d Version %d  \n", (ioapicread(REG_ID) >>24) & 0xFF,
                                           (ioapicread(REG_VER) ) & 0xF );
    

    kprintf("[IOAPIC] max interrupts %d \n", maxintr); 
#endif
    id = ioapicread(REG_ID) >> 24;
  
    // Mark all interrupts edge-triggered, active high, disabled,
    // and not routed to any CPUs.
    for(i = 0; i <= maxintr; i++){
      ioapicwrite(REG_TABLE+2*i, INT_DISABLED | (IRQ_OFFSET + i));
      ioapicwrite(REG_TABLE+2*i+1, 0);
    }
}


