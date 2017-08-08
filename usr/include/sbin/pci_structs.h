#pragma once

#if 0
#ifndef USER
#include <constants.h>
#endif
#include <stdint.h>

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

/***************************************************************************
 ***************************** PCI STRUCTS *********************************
 **************************************************************************/
#endif

#define PCI_CONTROL_DMA_MAST_EN  0x0004 


typedef struct{
	//Bar 0 is at 0x10, 1 at 0x14, 2 at 0x18, 3 at 0x1C, 
	//4 at 0x24, 5 at 0x28
	uint32_t bar;
	uint32_t bar_base;
	uint32_t bar_limit;	
	uint32_t bar_size;
	uint8_t  bar_type;	/* IO or mem */
#define PCI_BAR_MEM 0
#define PCI_BAR_IO  1
} Pci_bar_t;


typedef struct{
	uint16_t bus;
	uint16_t slot;
	uint16_t func;

	uint16_t dev_id;		//0x02
	uint16_t vendor;		//0x00
	uint16_t status;		//0x06
	uint16_t command;		//0x04
	
	uint8_t revision;		//0x08
	uint8_t prog_if;		//0x09
	uint8_t subclass;		//0x0A
	uint8_t class;		//0x0B 
	
	uint8_t bist;		//0x0F
	uint8_t header_type;	//0x0E
	uint8_t latency;		//0x0D
	uint8_t cache_line;	//0x0C	

	Pci_bar_t pci_dev_bars[6];	

	uint16_t subsys_id;	//0x2C
	uint16_t subsys_vendor;	//0x2E
	
	uint8_t max_latency;	//0x3F
	uint8_t min_grant;		//0x3E
	uint8_t interrupt_pin;	//0x3D 
	uint8_t interrupt_line;	//0x3C

} Pci_dev_t;

typedef struct{
	uint16_t bus;
	uint16_t slot;
	uint16_t func;

	uint16_t dev_id;		//0x02
	uint16_t vendor;		//0x00
	uint16_t status;		//0x06
	uint16_t command;		//0x04
	
	uint8_t revision;		//0x08
	uint8_t prog_if;		//0x09
	uint8_t subclass;		//0x0A
	uint8_t class;		//0x0B 
	
	uint8_t bist;		//0x0F
	uint8_t header_type;	//0x0E
	uint8_t latency;		//0x0D
	uint8_t cache_line;	//0x0C	

	Pci_bar_t pci_dev_bars[2];	

	uint8_t primary_bus;
	uint8_t secondary_bus;
	uint8_t subordinate_bus;
	uint8_t secondary_latency;
	uint8_t IO_base;
	uint8_t IO_limit; 
	uint16_t secondary_status;
	uint16_t memory_base;	
	uint16_t memory_limit; 
	uint16_t prefectch_mem_base;
	uint16_t prefetch_mem_limit;	
	
	uint32_t prefecth_base_upper32;
	uint32_t prefetch_limit_upper32;
	
	uint16_t IO_base_upper16;
	uint16_t IO_limit_upper16;
	
	uint8_t cap_pointer; 
	
	uint32_t expansion_rom_base; 
	uint16_t bridge_control;
	uint8_t interrupt_pin;	//0x3D 
	uint8_t interrupt_line;	//0x3C

} Pci_bridge_t;

