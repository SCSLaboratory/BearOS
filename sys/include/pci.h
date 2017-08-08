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

#include <constants.h>
#include <stdint.h>

/* user-land structure definitions */
#include <sbin/pci_structs.h>

/***************************************************************************
 ***************************** PCI DEFINES *********************************
 **************************************************************************/
#define MAX_PCI_BUSES 256
#define MAX_PCI_SLOTS 32
#define MAX_PCI_FUNCS 8
#define MAX_PCI_DEVS (MAX_PCI_BUSES*MAX_PCI_SLOTS*MAX_PCI_FUNCS)

/* PCI Capability List Types */
#define PCIY_PMG        0x01    /* PCI Power Management */
#define PCIY_AGP        0x02    /* AGP */
#define PCIY_VPD        0x03    /* Vital Product Data */
#define PCIY_SLOTID     0x04    /* Slot Identification */
#define PCIY_MSI        0x05    /* Message Signaled Interrupts */
#define PCIY_CHSWP      0x06    /* CompactPCI Hot Swap */
#define PCIY_PCIX       0x07    /* PCI-X */
#define PCIY_HT         0x08    /* HyperTransport */
#define PCIY_VENDOR     0x09    /* Vendor Unique */
#define PCIY_DEBUG      0x0a    /* Debug port */
#define PCIY_CRES       0x0b    /* CompactPCI central resource control */
#define PCIY_HOTPLUG    0x0c    /* PCI Hot-Plug */
#define PCIY_SUBVENDOR  0x0d    /* PCI-PCI bridge subvendor ID */
#define PCIY_AGP8X      0x0e    /* AGP 8x */
#define PCIY_SECDEV     0x0f    /* Secure Device */
#define PCIY_EXPRESS    0x10    /* PCI Express */
#define PCIY_MSIX       0x11    /* MSI-X */
#define PCIY_SATA       0x12    /* SATA */
#define PCIY_PCIAF      0x13    /* PCI Advanced Features */

#define PCI_HEADER_TYPE         0x0e    /* 8 bits */

/*Queue that maintains the list of PCIe controllers found on the system */
void * PCIe_root_cplxQ;

/***************************************************************************
 ************************** PUBLIC PCI FUNCTIONS ***************************
 **************************************************************************/
 

Pci_dev_t* pci_fill_struct(uint8_t bus, uint8_t slot, uint8_t func );

int pci_print_caps(Pci_dev_t *dev);



 void pci_print_dev(Pci_dev_t *pci_device);

#ifdef BOOTLOADER     /* Bootloader has a subset of PCI code. */

/* Scan PCI bus and pick out a IDE/ATA controller. */
 int pci_find_disk_controller(Pci_dev_t*);

#else                 /* Kernel, hypv, etc. have full PCI module. */

/* Init all PCI device info.  Called first */
 int pci_init();
 void pci_scan();

/* Acts on the queue of PCI devices. */
 Pci_dev_t *pci_search(int (*qsearchfn)(void*, const void*), void *key);
 void pci_apply  (void (*qapplyfn)(void*));
 void pci_apply2 (void *arg, void (*qapply2fn)(void *arg, void *elementp));

/* Functions for printing info to console. */
 void pci_print_dev(Pci_dev_t *pci_device);
 void pci_print_loc(int bus, int slot, int func);
 void pci_print_raw(uint8_t bus, uint8_t slot, uint8_t func); 

/* Read and write PCI config space for a device. */
 void     pci_write_config(Pci_dev_t*,unsigned,uint32_t,unsigned);
 uint32_t pci_read_config (Pci_dev_t*,unsigned,unsigned);

/* Convenience functions for accessing various PCI config fields */
 void pci_enable_busmaster(Pci_dev_t*);
 int  pci_find_cap(Pci_dev_t*,int,uint32_t*);

#endif /* ifdef BOOTLOADER */


