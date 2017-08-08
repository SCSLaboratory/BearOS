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
#include <pci.h>

#define APIC_ENTRY 0
#define IOAPIC_ENTRY 1
#define APIC_OVERRIDE_ENTRY 2

#define IOAPIC_TABLE_DMAR     0
#define IOAPIC_TABLE_RMRR     1
#define IOAPIC_TABLE_ASTR     2
#define IOAPIC_TABLE_RHSA     3
#define IOAPIC_TABLE_ANND     4
#define IOAPIC_TABLE_MAX      4 

#define IOAPIC_SCOPE_PCIDEV   1
#define IOAPIC_SCOPE_PCIBUS   2
#define IOAPIC_SCOPE_IOAPIC   3
#define IOAPIC_SCOPE_HPET     4
#define IOAPIC_SCOPE_ACIPNAME 5

struct RSDPDescriptor20 {
	char Signature[8];
	uint8_t Checksum;
	char OEMID[6];
	uint8_t Revision;
	uint32_t RsdtAddress;
	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t ExtendedChecksum;
	uint8_t reserved[3];
};

struct ACPIHeader {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	uint64_t OEMTableID;
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t RevisionID;
}__attribute__((packed));


struct ACPIDMAR {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	uint64_t OEMTableID;
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t RevisionID;
	uint8_t HostAddressWidth;
	uint8_t Flags;
	uint8_t Reserved[10];
}__attribute__((packed));

struct DMARHeader{
	uint16_t TableType;
	uint16_t Length;
};

struct DMAR_DRHD{
	uint16_t DeviceType;
	uint16_t Length;
	uint8_t Flags;
	uint8_t Reserved;
	uint16_t SegmentNumber;
	uint64_t RegisterBaseAddress;
};

struct DMAR_DeviceScope{
	uint8_t DeviceType;
	uint8_t Length;			
	uint16_t Reserved;
	uint8_t EnumerationID;
	uint8_t StartBusNumber;
};

struct DMAR_DevScopePath {
	uint8_t device;
	uint8_t function;
};

struct DMAR_RMRR {
	uint16_t DeviceType;
	uint16_t Length;
	uint16_t Reserved;
	uint16_t Segment;
	uint64_t BaseAddress;
	uint64_t EndAddress;
};

struct DMAR_ATSR {
	uint16_t DeviceType;
	uint16_t Length;
	uint8_t Flags;
	uint8_t Reserved;
	uint16_t SegmentNumber;	
};

struct DMAR_RHSA {
	uint16_t DeviceType;
	uint16_t Length;			
	uint32_t Reserved;
	uint64_t BaseAddress; 
	uint32_t ProxomityDomain; 
};

struct DMAR_ANDD {
	uint16_t DeviceType;
	uint16_t Length;
	uint16_t Reserved;
	uint8_t  Reserved2;
	uint8_t  ACPIDeviceNumber;
	char* 	name; 
};

struct qi_entry {
  uint64_t bits_lo;
  uint64_t bits_hi;
};

struct dmar_qi {
	struct qi_entry *qi_dscp;   /* the actual queue that lives in hardware */
	uint64_t        *qi_stat;   /* descriptor status */
	int             head;      /* first free entry */
	int             tail;      /* last free entry */
	int             count;
};


typedef struct _Dmar_t {
  uint16_t TableType;
	uint8_t DeviceType;
	Pci_dev_t  *dev; 
  struct DMAR_DRHD *drhd;
  struct DMAR_RMRR  *rmrr;
  uint64_t *cap_reg;
  uint64_t *ecap_reg;
  uint64_t *root_table_addr;
  uint64_t *iremap_table_addr;
  uint8_t HostAddrWidth;
  uint8_t IOAPIC_ID;
  uint16_t IOAPIC_SID;
  struct dmar_qi *qi;
}Dmar_t;


struct PCIeHeader {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	uint64_t OEMTableID;
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t RevisionID;
	uint64_t Reserved;
}__attribute__((packed));  //MUST be PACKED!!

struct PCIe_config
{
  uint64_t baseaddr;
  uint16_t segment_group;
  uint8_t  start_bus;
  uint8_t  end_bus;
  uint32_t reserved;
}__attribute__((packed)); //MUST BE PACKED


struct ACPIMADT {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	uint64_t OEMTableID;
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t RevisionID;
	uint32_t LocalAPICAddress;
	uint32_t Flags;
}__attribute__((packed));
	
struct APICHeader
{
	uint8_t DeviceType;
	uint8_t Length;
};

struct APICLocalAPICEntry
{
	uint8_t DeviceType;
	uint8_t Length;
	uint8_t ProcessorID;
	uint8_t APICID;
	uint32_t flags;
};

struct APICIOAPICEntry
{
	uint8_t DeviceType;
	uint8_t Length;
	uint8_t IOAPICId;
	uint8_t Reserved;
	uint32_t IOAPICAddress;
	uint32_t GlobalSystemInterruptBase;
};

struct APICInterruptOverrideEntry
{
	uint8_t DeviceType;
	uint8_t Length;
	uint8_t Bus;
	uint8_t Source;
	uint32_t Interrupt;
	uint16_t Flags;
};



struct ACPIHPETDHeader
{
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;  /* 16 bits to here */
    uint16_t pci_vendor_id:16;
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved2;
    uint64_t address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
}__attribute__((packed));



/* Global variable for the system's RSDP descriptor. */
extern struct RSDPDescriptor20 rsdpdesc;
extern void* CPUQueue;
extern void* IOAPICQueue;
extern void* InterruprOverRideQueue;

void scan_rsdp();
void Scan_ACPI();
