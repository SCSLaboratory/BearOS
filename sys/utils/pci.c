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

#include <pci.h>
#include <pio.h>
#include <stdint.h>
#include <kstdio.h>
#include <kstring.h>

#ifndef BOOTLOADER
#include <kmalloc.h>
#include <kqueue.h>
#endif

/* PRIVATE DECLARATIONS */

#ifndef BOOTLOADER
/* 
 * Queue of devices.
 * This queue is filled during pci_init().  During system init, drivers will 
 * search the queue for their device, or alternatively, use an "apply"
 * function to look for multiple devices usable by the driver.
 */
void *devq;
#endif



/* Reading pci config data */
uint8_t  pci_read8  (uint8_t,uint8_t,uint8_t,uint8_t);
uint16_t pci_read16 (uint8_t,uint8_t,uint8_t,uint8_t);
uint32_t pci_read32 (uint8_t,uint8_t,uint8_t,uint8_t);

/* Writing pci config data */

static void pci_write8  (uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static void pci_write16 (uint8_t,uint8_t,uint8_t,uint8_t,uint16_t);
static void pci_write32 (uint8_t,uint8_t,uint8_t,uint8_t,uint32_t);

/***************************************************************************
 ***************************** PUBLIC FUNCTIONS ****************************
 **************************************************************************/

#ifndef BOOTLOADER
/****************************************************************************
 * Function: pci_init_dev_array()
 *
 * Description: Returns an array of pointers to Pci_dev_ts. These are all
 *              the PCI devices found on the system.
 *
 ***************************************************************************/
int pci_init() {
  uint32_t bus, slot, func, vendor, type;
  Pci_dev_t *dev_ptr;

  /* Init the queue */
  devq = qopen();

  /* Now, go find all devices. */
  for (bus = 0; bus < MAX_PCI_BUSES  ; bus++) {
    for (slot = 0; slot < MAX_PCI_SLOTS ; slot++) {
      for (func = 0; func < MAX_PCI_FUNCS; func++) {

	/* Is this an empty slot? */
	vendor = pci_read16(bus, slot, func, 0x00);
	if (vendor == 0xffff ) continue;

	/* Is this a device type we care about? */
	type = pci_read8(bus, slot,func,PCI_HEADER_TYPE);
	if((type == 0x00) || (type == 0x80)) {

	  /* Get all dev data and put into queue. */
	  dev_ptr = pci_fill_struct(bus, slot, func);
	  qput(devq, dev_ptr);	
	}
	if(type == 0x1){
#ifdef DEBUG
	  kprintf("Found secondary bus at bus %d, dev %d, func %d\n", bus, slot, func);
#endif
	}
      }//functions loop
    }//slots loop
  }//bus loop

  return 0;
}

Pci_dev_t *pci_search(int (*qsearchfn)(void*, const void*), void *key) {
  return qsearch(devq, qsearchfn, key);
}

void pci_apply(void (*qapplyfn)(void*)) {
  qapply(devq, qapplyfn);
}

void pci_apply2(void *arg, void (*qapply2fn)(void *arg, void *elementp)) {
  qapply2(devq, arg, qapply2fn);
}

/****************************************************************************
 * Function: print_device_ptr
 *
 * Description: Prints the device info for the given device
 *
 ***************************************************************************/
void pci_print_dev(Pci_dev_t *pci_device) {
  int j;
  kprintf("Pci device BUS: 0x%x SLOT: 0x%x FUNC: 0x%x ",
	  pci_device->bus,pci_device->slot,pci_device->func);
  kprintf("ID: 0x%x VENDOR: 0x%x\nStatus: 0x%x Cmd: 0x%x Rev: 0x%x ",
	  pci_device->vendor,pci_device->status,pci_device->command,pci_device->revision);

  kprintf("Class: 0x%x Subclass: 0x%x PIF: 0x%x\n",
	  pci_device->class,pci_device->subclass,pci_device->prog_if);
  kprintf("BIST: 0x%x header_type: 0x%x latency: 0x%x cache_line: 0x%x\n",
	  pci_device->bist,pci_device->header_type,pci_device->latency,pci_device->cache_line);

  kprintf("subsys_ID: 0x%x , subsys_vendor: 0x%x, IRQ: 0x%x\n",
	  pci_device->subsys_id,pci_device->subsys_vendor,pci_device->interrupt_line);

  for(j=0;j<6;j++) {
    if(pci_device -> pci_dev_bars[j].bar != 0) {
      kprintf(" Device BAR # 0x%x ", j);
      kprintf(" Base address 0x%x; ",  pci_device -> pci_dev_bars[j].bar_base);
      kprintf(" Limit 0x%x \n",  pci_device -> pci_dev_bars[j].bar_limit);
    }
  }
}

/****************************************************************************
 * Function: print_device_info
 *
 * Description: Prints the device info for the given bus, slot, and function
 *
 ***************************************************************************/
void pci_print_loc(int bus, int slot, int func) {
  Pci_dev_t *pci_device;
  pci_device = pci_fill_struct(bus, slot, func);
  pci_print_dev(pci_device);
  kfree_track(PCI_SITE,pci_device);
}

/****************************************************************************
 * Function: print_raw_device_info
 *
 * Description: Prints out the raw bytes of PCI info for the given
 *              bus, slot, and function.
 *
 ***************************************************************************/
void pci_print_raw(uint8_t bus, uint8_t slot, uint8_t func) {
  int i, j;
  unsigned int value;

  kprintf("pci 0000:%02x:%02x.%d config space:",bus, slot, func);

  for (i = 0; i < 256; i += 4) {
    //if (!(i & 0x0f))
    kprintf("\n  %02x:",i);

    value = pci_read32(bus, slot, func, i);
    for (j = 0; j < 4; j++) {
      kprintf(" %02x", value & 0xff);
      value >>= 8;
    }
  }
  kprintf("\n");
}


/****************************************************************************
 * Function: pci_write_config
 *
 * Description: Given a device pointer, writes to that device's config
 *              space.
 ***************************************************************************/
void pci_write_config(Pci_dev_t *dev, unsigned offset, uint32_t val, 
                      unsigned width) {
  unsigned b,d,f; /* Bus, Device, Function */
  b = dev->bus;
  d = dev->slot;
  f = dev->func;

  switch(width) {
  case 4:
    pci_write32(b,d,f,offset,val);
    break;
  case 2:
    pci_write16(b,d,f,offset,(uint16_t)(val & 0xFFFF));
    break;
  case 1:
    pci_write8(b,d,f,offset,(uint8_t)(val & 0xFF));
  default:
    break;
  }
}

/****************************************************************************
 * Function: pci_write_config
 *
 * Description: Given a device pointer, writes to that device's config
 *              space.
 ***************************************************************************/
uint32_t pci_read_config(Pci_dev_t *dev, unsigned offset, unsigned width) {
  uint32_t val;
  unsigned b,d,f; /* Bus, Device, Function */
  b = dev->bus;
  d = dev->slot;
  f = dev->func;

  switch(width) {
  case 4:
    val = pci_read32(b,d,f,offset);
    break;
  case 2:
    val = (uint32_t)pci_read16(b,d,f,offset);
    break;
  case 1:
    val = (uint32_t)pci_read8(b,d,f,offset);
    break;
  default:
    val = 0;
    break;
  }

  return val;
}


/****************************************************************************
 * Function: pci_enable_busmaster()
 *
 * Description: Given a device pointer, enables busmastering for that
 *              device.
 * TODO: Fix magic numbers
 ***************************************************************************/
void pci_enable_busmaster(Pci_dev_t *dev) {
  uint16_t cmd_reg;
  if (dev == NULL) return;

  cmd_reg = (uint16_t)pci_read_config(dev,0x4,2);
  cmd_reg |= 0x4;
  pci_write_config(dev,0x4,cmd_reg,2);
}


/****************************************************************************
 * Function: pci_find_cap()
 *
 * Description: Given a device pointer and a capability type, this will try
 *              to find the corresponding PCI capability listing in a
 *              device's "PCI Capabilities," a linked list that may or may
 *              not exist in the PCI config space, and which may or may not
 *              contain a listing for any given capability.
 * 
 ***************************************************************************/
#define PCI_STATUS_CAPLIST_MASK 0x10
#define MAX_CAPS 0x50
int pci_find_cap(Pci_dev_t *dev, int cap_wanted, uint32_t *capreg_offset) {
  int i;
  uint8_t cl_offset, next_offset;

  /* First get status reg and see if it supports PCI cap list */
  if (((dev->status) & PCI_STATUS_CAPLIST_MASK) == 0) {
    kprintf("Error: No Capability list not supported\n");
    return -1;
  }
  if ((dev->header_type & 0x7) != 0) {
    kprintf("Error: Header type is %d not 0x0\n",dev->header_type & 0x7);
    return -1;
  }

  /* Go find start of caplist */
  cl_offset = pci_read_config(dev,0x34,1);
  cl_offset &= ~0x3;                       /* bits 0-1 are reserved */

  /* Search until cap is found, end encountered, or max caps reached */
  for(i=0; i<MAX_CAPS; i++,cl_offset = next_offset) {
    uint8_t cap;

    if (cl_offset < 0x40) {
      kprintf("ERROR: Illegal offset encountered during PCI capsearch\n");
      *capreg_offset = 0;
      return -1;
    }
    cap = pci_read_config(dev,cl_offset,1);
    if (cap == cap_wanted) {
      *capreg_offset = cl_offset;
      return 0;
    }

    next_offset = pci_read_config(dev,cl_offset+1,1);
    next_offset &= ~0x3;

    if (cap == 0xff) break;
    if (next_offset == 0) break;
  }

  *capreg_offset = 0;
  return -1;
}

int pci_print_caps(Pci_dev_t *dev) {
  int i;
  uint8_t cl_offset, next_offset;

  /* First get status reg and see if it supports PCI cap list */
  if (((dev->status) & PCI_STATUS_CAPLIST_MASK) == 0) {
    kprintf("Error: No Capability list not supported\n");
    return -1;
  }
  if ((dev->header_type & 0x7) != 0) {
    kprintf("Error: Header type is %d not 0x0\n",dev->header_type & 0x7);
    return -1;
  }

  /* Go find start of caplist */
  cl_offset = pci_read_config(dev,0x34,1);
  cl_offset &= ~0x3;                       /* bits 0-1 are reserved */

  /* Search until cap is found, end encountered, or max caps reached */
  for(i=0; i<MAX_CAPS; i++,cl_offset = next_offset) {
    uint8_t cap;

    if (cl_offset < 0x40) {
      kprintf("ERROR: Illegal offset encountered during PCI capsearch\n");
      return -1;
    }
    cap = pci_read_config(dev,cl_offset,1);
      kprintf("found pci capability %x \n", cap);

    next_offset = pci_read_config(dev,cl_offset+1,1);
    next_offset &= ~0x3;

    if (cap == 0xff) break;
    if (next_offset == 0) break;
  }

  
  return -1;
}



#endif /* ifndef BOOTLOADER */


#ifdef BOOTLOADER

int pci_find_disk_controller(Pci_dev_t *dev) {
  unsigned int bus, slot, func;
  unsigned int class, subclass, vendor;

  /* go find all devices. */
  for (bus = 0; bus < MAX_PCI_BUSES  ; bus++) {
    for (slot = 0; slot < MAX_PCI_SLOTS ; slot++) {
      for (func = 0; func < MAX_PCI_FUNCS; func++) {

	/* Is this an empty slot? */
	vendor = pci_read8(bus, slot, func, 0x00);
	if (vendor == 0xffff || vendor == 0x0000) continue;

	/* Is this a device type we care about? */
	class = pci_read8(bus, slot,func,0xB);
	subclass = pci_read8(bus,slot,func,0xA);
	if((class == 0x01) && (subclass == 0x01)) {
	  pci_fill_struct(bus, slot, func,dev);
	  return 0;

	} //type test if
      }//functions loop
    }//slots loop
  }//bus loop
  return 1;
}

#endif /* ifdef BOOTLOADER */

/**********************************************************************
 ********************* PRIVATE HELPER FUNCTIONS ***********************
 *********************************************************************/

Pci_dev_t* pci_fill_struct(uint8_t bus, uint8_t slot, uint8_t func )

{
  int i, j;
  uint32_t base, x;
  uint32_t addr_mask;
  int bar_type;

  Pci_dev_t* pci_dev = (Pci_dev_t*)kmalloc_track(PCI_SITE,sizeof(Pci_dev_t));
  if(!pci_dev){
    kprintf("error no memory avil");
    return NULL;
  }	


  kmemset(pci_dev,0,sizeof(Pci_dev_t));

  pci_dev -> bus = bus;
  pci_dev -> slot = slot;
  pci_dev -> func = func;

  pci_dev -> vendor  = pci_read16(bus, slot, func, 0x00);
  pci_dev -> dev_id  = pci_read16(bus, slot, func, 0x02);
  pci_dev -> command = pci_read16(bus, slot, func, 0x04);
  pci_dev -> status  = pci_read16(bus, slot, func, 0x06);
	
  pci_dev -> revision	   = pci_read8(bus, slot, func, 0x08);
  pci_dev -> prog_if     = pci_read8(bus, slot, func, 0x09);
  pci_dev -> subclass    = pci_read8(bus, slot, func, 0x0A);
  pci_dev -> class       = pci_read8(bus, slot, func, 0x0B);
  pci_dev -> cache_line  = pci_read8(bus, slot, func, 0x0C);
  pci_dev -> latency     = pci_read8(bus, slot, func, 0x0D);
  pci_dev -> header_type = pci_read8(bus, slot, func, 0x0E);
  pci_dev -> bist        = pci_read8(bus, slot, func, 0x0F);
	
  pci_dev -> subsys_id       = pci_read16(bus, slot, func, 0x2C);
  pci_dev -> subsys_vendor   = pci_read16(bus, slot, func, 0x2E);
  pci_dev -> max_latency     = pci_read8(bus, slot, func, 0x3F);
  pci_dev -> min_grant       = pci_read8(bus, slot, func, 0x3E);
  pci_dev -> interrupt_pin   = pci_read8(bus, slot, func, 0x3D);
  pci_dev -> interrupt_line  = pci_read8(bus, slot, func, 0x3C);

  pci_dev -> pci_dev_bars[0].bar = pci_read32(bus, slot, func, 0x10);
  pci_dev -> pci_dev_bars[1].bar = pci_read32(bus, slot, func, 0x14);
  pci_dev -> pci_dev_bars[2].bar = pci_read32(bus, slot, func, 0x18);
  pci_dev -> pci_dev_bars[3].bar = pci_read32(bus, slot, func, 0x1C);
  pci_dev -> pci_dev_bars[4].bar = pci_read32(bus, slot, func, 0x20);
  pci_dev -> pci_dev_bars[5].bar = pci_read32(bus, slot, func, 0x24);
	
  /* 
   * Test each bar; if it's something greater than zero determine if it's
   *  io or mem and find the limits.
   * i indexes the 6 bars, j indexes their hex value by 4 bytes each time
   */
  for(i=0,j=0x10;i<6;i++,j+=0x04){
    uint32_t bar_reg = pci_dev -> pci_dev_bars[i].bar;
    if(bar_reg > 0){ /* greater than zero aka valid */

      /* Determine and save the bar type. It's bit 1 of bar_reg */
      bar_type = bar_reg & 0x1;
      pci_dev->pci_dev_bars[i].bar_type = bar_type;

      /* Bar type determines the layout of the other bar bits. */
      if(bar_type == PCI_BAR_IO) {
	addr_mask = 0xFFFFFFFC;
      } else { /* bar_type == PCI_BAR_MEM */
	addr_mask = 0xFFFFFFF0;
      }

      /*
       * The first few bits (how many depends on bar type) aren't part
       * of the base address.  Have to mask them out.
       */
      base = bar_reg & addr_mask;
      pci_dev -> pci_dev_bars[i].bar_base = base;

      /*
       * write all F's to get size, then follow the instructions.
       * Work some magic and then put the value back!!
       */
      pci_write32(bus,slot,func,j , (uint32_t)(0xFFFFFFFF));	
      x = pci_read32(bus, slot, func, j);				
      x = ~(x & addr_mask);
      pci_write32(bus,slot,func, j,base+1);	
				
      /* Now we can get the upper limit and size */
      pci_dev -> pci_dev_bars[i].bar_limit = (unsigned short)(base + x);
      pci_dev -> pci_dev_bars[i].bar_size = x + 1;
    }
  }
  /* (ST) Prototype is void if bootloader*/
#ifndef BOOTLOADER
  return pci_dev;
#endif
}	

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, 
		  uint8_t offset) {
  unsigned char v;
  outl(0xcf8, 0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  v = inb(0xcfc + (offset&3));
  return v;
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, 
		    uint8_t offset) {
  unsigned short v;
  outl(0xcf8,0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  v = inw(0xcfc + (offset&2));
  return v;
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, 
		    uint8_t offset) {
  unsigned int v;
  outl(0xcf8,0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  v = inl(0xcfc);
  return v;
}


static void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, 
		       uint8_t offset, uint8_t value) {
  outl(0xcf8, 0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  outb(0xcfc + (offset&3) , value );
        
}


static void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, 
			uint8_t offset, uint16_t value) {
  outl(0xcf8, 0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  outw(0xcfc + (offset&2) , value );
}

static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, 
			uint8_t offset, uint32_t value) {
  outl(0xcf8, 0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset);
  outl(0xcfc , value );
}
