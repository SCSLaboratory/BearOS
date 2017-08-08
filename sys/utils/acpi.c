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

#include <kstring.h>
#include <stdint.h>
#include <acpi.h>
#include <kqueue.h>
#include <kvmem.h>
#include <smp.h>
#include <kmalloc.h>
#include <apic.h>
#include <pci.h>

/*Not all debugging features are enabled by default in this file (RMD)*/

struct RSDPDescriptor20 rsdpdesc;
void* CPUQueue;
void* IOAPICQueue;
void* InterruptOverRideQueue;
void* DMARDEVQueue;

void scan_rsdp() {
	const char *rsdptr = "RSD PTR ";
	uint64_t i;

	kmemset(&rsdpdesc, 0, sizeof(struct RSDPDescriptor20));
	for(i = 0x9fc00; i < 0xa0000; i += 16) {
		if(kstrncmp((char *)i, rsdptr, kstrlen(rsdptr)) == 0) {
			kmemcpy(&rsdpdesc, (void *)i, sizeof(struct RSDPDescriptor20));
			return;
		}
	}

	for(i = 0xe0000; i < 0x100000; i += 16) {
		if(kstrncmp((char *)i, rsdptr, kstrlen(rsdptr)) == 0) {
			kmemcpy(&rsdpdesc, (void *)i, sizeof(struct RSDPDescriptor20));
			return;
		}
	}

	kputs("[ACPI] RSDP not found!");
}


/* If you need to print a table you can use this code (RMD) */
   /* Note you will get part of the next table	
   uint8_t* print;

   while(entry < end_dt)
   {
   print = (uint8_t *)entry;
   kprintf("%x %x %x %x %x",*print,*(print+1),*(print+2),*(print+3),*(print+4));
   entry+=5;
   print = (uint8_t *)entry;
   kprintf(" %x %x %x %x %x",*print,*(print+1),*(print+2),*(print+3),*(print+4));
   entry+=5;
   print = (uint8_t *)entry;
   kprintf(" %x %x %x %x %x",*print,*(print+1),*(print+2),*(print+3),*(print+4));
   entry+=5;
   print = (uint8_t *)entry;
   kprintf(" %x\n",*print);
   entry++;
   header = (struct APICHeader *)entry;
   }
*/

void ACPI_Parse_HPET(struct ACPIHeader *acpiheader){
  struct ACPIHPETDHeader *hpetheader;
	
	hpetheader = (struct ACPIHPETDHeader *)((uint8_t *)acpiheader + sizeof(struct ACPIHeader));
#ifdef DEBUG_ACPI	
	kprintf("Hpet hardware rev %d \n", hpetheader->hardware_rev_id);
	kprintf("Hpet count %d \n", hpetheader->comparator_count);
	kprintf("Hpet counter size %d \n", hpetheader->counter_size);
	kprintf("Hpet legacy   %d \n", hpetheader->legacy_replacement);
	kprintf("hept pci vendor id %d \n", hpetheader->pci_vendor_id);
	kprintf("Hpet addr id   %d \n", hpetheader->address_space_id);
	kprintf("Hpet reg bit width   %d \n", hpetheader->register_bit_width);
	kprintf("Hpet reg bit offset   %d \n", hpetheader->register_bit_offset);
	kprintf("Hpet address  %x \n", hpetheader->address);	
	kprintf("Hpet number    %d \n", hpetheader->hpet_number);
	kprintf("Hpet min tick   %d \n", hpetheader->minimum_tick);
	kprintf("Hpet page protection   %d \n", hpetheader->page_protection);
	kprintf("\n");
	kprintf("\n");
#endif
}


void parse_rmrr(struct DMARHeader *dmar_header, struct DMAR_RMRR *rmrr_header)
{
  uint8_t *scope_start, *scope_end;
	uint8_t *pathstart, *pathend;
 	Pci_dev_t *dev_ptr;
  struct DMAR_DeviceScope *scope_header;
	struct DMAR_DevScopePath  *path_header; 
	int path_length;
  Dmar_t *dmar_device;
#ifdef DEBUG_DMAR  
  kprintf(" Reserved memory region starts at 0x%x ends at 0x%x \n", rmrr_header->BaseAddress, rmrr_header->EndAddress);
  kprintf(" Rmrr size 0x%x %d\n", rmrr_header->EndAddress - rmrr_header->BaseAddress + 1, rmrr_header->Segment);																					
																						
#endif 																						
			
	scope_start =  (uint8_t*)dmar_header + sizeof(struct DMAR_RMRR);
	scope_end	  =  (uint8_t *)dmar_header + dmar_header->Length;
				 
	while(scope_start < scope_end)
	{
		scope_header = (struct DMAR_DeviceScope *)scope_start;
#ifdef DEBUG_DMAR		 
		kprintf("  Device type: %d, ", scope_header->DeviceType);
#endif

		path_length = (scope_header->Length - sizeof(struct DMAR_DeviceScope)) / sizeof(struct DMAR_DevScopePath); 
		pathstart = scope_start + sizeof(struct DMAR_DeviceScope); 
		pathend = pathstart + path_length / sizeof(struct DMAR_DevScopePath); 
		path_header = (struct DMAR_DevScopePath *)pathstart; 
		do
		{
			if(scope_header->DeviceType == IOAPIC_SCOPE_PCIDEV)
			{
				dev_ptr = pci_fill_struct(scope_header->StartBusNumber, path_header->device, path_header->function);
				//pci_print_dev(dev_ptr);
				dmar_device = (Dmar_t *)kmalloc_track(ACPI_SITE,sizeof(Dmar_t));
				dmar_device->TableType =  dmar_header->TableType;
				dmar_device->DeviceType = scope_header->DeviceType;
				dmar_device->drhd =       NULL;
				dmar_device->rmrr =       rmrr_header;
				dmar_device->dev  =       dev_ptr; 
				
				qput(DMARDEVQueue,(void *)dmar_device);
#ifdef DEBUG_DMAR			
			  kprintf("Bus: %d Device: %d, Function: %d\n",
					     scope_header->StartBusNumber, path_header->device, path_header->function);
				//pci_print_dev(dev_ptr);	            
#endif
			}//if close
			if(scope_header->DeviceType != IOAPIC_SCOPE_PCIDEV)
			{
			  kprintf("rmrr with device type %d Not supported \n", scope_header->DeviceType);
			}				
			pathstart += sizeof(struct DMAR_DevScopePath);
		}while(pathstart < pathend);
		
		/*jump to the next */
		scope_start += scope_header->Length;
	}//scope while loop end
}


void parse_drhd(struct ACPIDMAR *dmar, struct DMARHeader *dmar_header, struct DMAR_DRHD *drhd_header)
{
	uint8_t *scope_start, *scope_end;
	uint8_t *pathstart, *pathend;
 	Pci_dev_t *dev_ptr;
  struct DMAR_DeviceScope *scope_header;
	struct DMAR_DevScopePath  *path_header; 
	int path_length;
  Dmar_t *dmar_device;
#ifdef DEBUG_DMAR
	kprintf(" MMIO Register Address 0x%x, include all 0x%x\n", 
						drhd_header->RegisterBaseAddress, drhd_header->Flags);
#endif			
	scope_start =  (uint8_t*)dmar_header + sizeof(struct DMAR_DRHD);
	scope_end	  =  (uint8_t *)dmar_header + dmar_header->Length;
			
				
	while(scope_start < scope_end)
	{
	  scope_header = (struct DMAR_DeviceScope *)scope_start;
#ifdef DEBUG_DMAR	   
		kprintf("  Device type %d: ", scope_header->DeviceType);
#endif
    /*dev type 1 is pcie endpoint devices */				
		if(scope_header->DeviceType == IOAPIC_SCOPE_PCIDEV)
		{
			/*how many devices are hidden here */ 
			path_length = (scope_header->Length - sizeof(struct DMAR_DeviceScope)) / sizeof(struct DMAR_DevScopePath);
			pathstart = scope_start + sizeof(struct DMAR_DeviceScope); 
			pathend = pathstart + path_length / sizeof(struct DMAR_DevScopePath); 
			path_header = (struct DMAR_DevScopePath *)pathstart; 
		
			do
			{
 				dev_ptr = pci_fill_struct(scope_header->StartBusNumber, path_header->device, path_header->function);
						
				dmar_device = (Dmar_t *)kmalloc_track(ACPI_SITE,sizeof(Dmar_t));
				dmar_device->TableType =  dmar_header->TableType;
				dmar_device->DeviceType = scope_header->DeviceType;
				dmar_device->drhd =       drhd_header;
				dmar_device->rmrr =       NULL;
				dmar_device->dev  =       dev_ptr; 
				dmar_device->HostAddrWidth = dmar->HostAddressWidth;
				qput(DMARDEVQueue,(void *)dmar_device);
#ifdef DEBUG_DMAR			
			  kprintf("Bus: %d, Device: %d, Function: %d\n",
					     scope_header->StartBusNumber, path_header->device, path_header->function);
				//pci_print_dev(dev_ptr);	            
#endif			
				
				pathstart += sizeof(struct DMAR_DevScopePath);
			}while(pathstart < pathend);
		}
		/*type 3 is the IOAPIC type devices and 4 is the HPET */
		if(scope_header->DeviceType == IOAPIC_SCOPE_IOAPIC  || scope_header->DeviceType == IOAPIC_SCOPE_HPET )
		{
			path_length = (scope_header->Length - sizeof(struct DMAR_DeviceScope));
			pathstart = (uint8_t*)(scope_start + sizeof(struct DMAR_DeviceScope)); 
			pathend =   (uint8_t *)(pathstart + path_length); 
			
			
			/*yes this is magic defined by the intel spec. If the path legth is two 
			 *this identifies the device as an IOAPIC  
			 */
			if(path_length == 2)
			{ 
				path_header = (struct DMAR_DevScopePath *)pathstart; 
				dev_ptr = pci_fill_struct(drhd_header->SegmentNumber, path_header->device, path_header->function);
				             
				dmar_device = (Dmar_t *)kmalloc_track(ACPI_SITE,sizeof(Dmar_t));
         
        if(scope_header->DeviceType == IOAPIC_SCOPE_IOAPIC )
        {
          /*this is the source id programmed later for interrupt remapping */        
          dmar_device->IOAPIC_SID = ((drhd_header->SegmentNumber) << 8) | 
                                    ((path_header->device) << 3) | 
                                      path_header->function;
          /*in systems w/ more than 1 ioapic the ID is needed */                            
 				  dmar_device->IOAPIC_ID  = scope_header->EnumerationID;	
#ifdef DEBUG 				  
          kprintf("[DMAR] IOAPIC ID %d\n", dmar_device->IOAPIC_ID);
          kprintf("[DMAR] IOAPIC SID %d\n", dmar_device->IOAPIC_SID);
#endif
        }     
  			dmar_device->TableType  = dmar_header->TableType;
				dmar_device->DeviceType = scope_header->DeviceType;
				dmar_device->drhd =       drhd_header;
				dmar_device->rmrr =       NULL;
				dmar_device->dev  =       dev_ptr; 
        dmar_device->HostAddrWidth = dmar->HostAddressWidth;
				qput(DMARDEVQueue,(void *)dmar_device);     
#ifdef DEBUG_DMAR			
				kprintf("   bus %d device %d, function %d\n",
				        drhd_header->SegmentNumber, path_header->device, path_header->function); 
				//pci_print_dev(dev_ptr);             
#endif				        
				             
			}
			/*if the path length is >2 then its' a PCI bridge */				
			if(path_length > 2)
			  kprintf("[DMAR] Warning IOAPIC behind pci bridge. NOT SUPPORTED!! \n");
		}
				
		/*jump to the next */
	  scope_start += scope_header->Length;
	}//scope while loop
		
}

void ACPI_Parse_DMAR(struct ACPIDMAR *dmar){
	uint8_t *entry, *end_dt;
	uint8_t *scope_start, *scope_end;
	struct DMARHeader *dmar_header;
	
	struct DMAR_DeviceScope *scope_header;

	
	struct DMAR_DRHD *drhd_header;
	struct DMAR_RMRR *rmrr_header;	
	struct DMAR_ATSR *atsr_header;
	struct DMAR_RHSA *rhsa_header; 
	struct DMAR_ANDD *andd_header; 

	
	DMARDEVQueue = qopen(); 	
#ifdef DEBUG	
	kprintf("IOMMU host address width %d \n", dmar->HostAddressWidth);
#endif
	/*The tables start at the end of the intial dmar table */
	entry = (uint8_t *)dmar + sizeof(struct ACPIDMAR);
	/*total size of the dmar tables */
	end_dt = (uint8_t *)dmar+dmar->Length;
	/*Debugging*/
	//kprintf("dmar = %x start = %x end = %x size = %x length = %x\n\n",dmar,entry,end_dt,sizeof(struct ACPIDMAR),dmar->Length);

	while(entry < end_dt){
		/* First cast it to the generic header so we can determine the type */
		dmar_header = (struct DMARHeader *)entry;
						
		if(dmar_header->TableType == IOAPIC_TABLE_DMAR)
		{
#ifdef DEBUG_DMAR		
			kprintf("\n[DMAR] Found DRHD table type %d, Length %x\n", dmar_header->TableType, dmar_header->Length);
#endif			
			/*okay now recast the entry to the correct type so we can access it's parts */
			drhd_header = (struct DMAR_DRHD *)entry;
			parse_drhd(dmar, dmar_header, drhd_header);
		}
				
		else if(dmar_header->TableType == IOAPIC_TABLE_RMRR)
		{
#ifdef DEBUG_DMAR
			kprintf("\n[DMAR] Found RMRR table type %d \n", dmar_header->TableType);
#endif			
			rmrr_header = (struct DMAR_RMRR *)entry;
			parse_rmrr(dmar_header, rmrr_header); 
			
		}//type 1 end		
		
		else if(dmar_header->TableType == IOAPIC_TABLE_ASTR)
		{
#ifdef DEBUG_DMAR		
			kprintf("\n[DMAR] Found ASTR table type %d \n", dmar_header->TableType);
#endif			
			atsr_header = (struct DMAR_ATSR*)entry; 
			
			/*not the PITA each entry has a variable number of devices */
			scope_start =  (uint8_t*)dmar_header + sizeof(struct DMAR_ATSR);
			scope_end	  =  (uint8_t *)dmar_header + dmar_header->Length; 
			
			while(scope_start < scope_end){
				scope_header = (struct DMAR_DeviceScope *)scope_start; 
					//kprintf("found device type %d \n", scope_header->DeviceType);
				scope_start += scope_header->Length;
			}	
			
		}//type 2 end
		else if(dmar_header->TableType == IOAPIC_TABLE_RHSA){
#ifdef DEBUG_DMAR
			kprintf("\n[DMAR] Found RHSA table type %d \n", dmar_header->TableType);
#endif
			rhsa_header = (struct DMAR_RHSA*)entry; 
		}
		else if(dmar_header->TableType == IOAPIC_TABLE_ANND){
#ifdef DEBUG_DMAR
			kprintf("\n[DMAR] Found ANND table type %d \n", dmar_header->TableType);
#endif
			andd_header = (struct DMAR_ANDD*)entry; 
		}
		else if(dmar_header->TableType > IOAPIC_TABLE_MAX){
			//kprintf("\n   future case 4+\n");
		}
		entry+= dmar_header->Length;
	}	
#ifdef DEBUG_DMAR
  kprintf("\n");
#endif 	
}



void ACPI_Parse_MADT(struct ACPIMADT *madt)
{
	uint8_t *entry, *end_dt;
	struct APICHeader *header;
	struct APICIOAPICEntry *ioapic;
	
#ifdef ENABLE_SMP
	struct APICLocalAPICEntry *apic;
	struct APICInterruptOverrideEntry *interruptoverride;
	void *Qvalptr;
	CPUQueue = qopen();
	IOAPICQueue = qopen();
	InterruptOverRideQueue = qopen();
#else
	smp_num_cpus = 1;
#endif
	lapicaddr = madt->LocalAPICAddress;
	/*Debugging*/
	/*kprintf("Signature = %s length = %x local apic address = %x\n",madt->Signature,madt->Length,madt->LocalAPICAddress);
	*/
	entry = (uint8_t *)madt + sizeof(struct ACPIMADT); 
	end_dt = (uint8_t *)madt+madt->Length;
	/*Debugging*/
	/*kprintf("madt = %x start = %x end =%x size = %x\n",madt,entry,end_dt,sizeof(struct ACPIMADT));
	*/

	/*we look bit by bit for madt entries, not the greatest solution, but it works*/
	while(entry < end_dt)
	{
		/* This is an example debug line, redelcare variable if used
		print = (uint8_t *)entry;
		kprintf("%x %x %x %x %x %x %x %x\n",*print,*(print+1),*(print+2),*(print+3),*(print+4),*(print+5),*(print+6),*(print+7));
		*/
		header = (struct APICHeader *)entry;
#ifdef ENABLE_SMP
		if(header->DeviceType == APIC_ENTRY && header->Length == 8)
		{
			apic = (struct APICLocalAPICEntry*)header;
			
			/* Cool vmware note. They set the apic reigster flags to mark usable and unusable*/
			if((apic->flags & 0x1) == 1){
	#ifdef DEBUG_ACPI
				kprintf("[ACPI] Found APIC, Length = %x Processor ID = %x APIC ID = %x\n",apic->Length,apic->ProcessorID,apic->APICID);
	#endif

				Qvalptr = kmalloc_track(ACPI_SITE,sizeof(struct APICLocalAPICEntry));
				Qvalptr = (void*)apic;
				qput(CPUQueue,Qvalptr);
		    smp_num_cpus++;
			}
		}
		else if(header->DeviceType == IOAPIC_ENTRY && header->Length == 12)
		{
			ioapic = (struct APICIOAPICEntry*)header;
			ioapicaddr = ioapic->IOAPICAddress; /*FIXME: this is bad news on a server*/

#ifdef DEBUG_ACPI
			kprintf("[ACPI] ioapic address = %x\n",ioapic->IOAPICAddress);
			kprintf("[ACPI] ioapic ID = %x\n",ioapic->IOAPICId);
#endif
			Qvalptr = kmalloc_track(ACPI_SITE,sizeof(struct APICIOAPICEntry));
			Qvalptr = (void*)ioapic;
			qput(IOAPICQueue,Qvalptr);
		}
		else if(header->DeviceType ==  APIC_OVERRIDE_ENTRY && header->Length == 10)
		{
			interruptoverride = (struct APICInterruptOverrideEntry*)header;
#ifdef DEBUG_ACPI
			kprintf("[ACPI] found interrupt override interrupt %x\n",interruptoverride->Interrupt);
#endif
			Qvalptr = kmalloc_track(ACPI_SITE,sizeof(struct APICInterruptOverrideEntry));
			Qvalptr = (void*)interruptoverride;
			qput(InterruptOverRideQueue,Qvalptr);
		}
#else
		if(header->DeviceType == IOAPIC_ENTRY && header->Length == 12)
		{
			ioapic = (struct APICIOAPICEntry*)header;
			ioapicaddr = ioapic->IOAPICAddress; /*FIXME: this is bad news on a server*/	
#ifdef DEBUG_ACPI
			kprintf("[ACPI] ioapic address = %x\n",ioapic->IOAPICAddress);
			kprintf("[ACPI] ioapic ID = %x\n",ioapic->IOAPICId);
#endif
		}
#endif

		entry++;
	}
	

	
/* debug info for our newly created queues of useful information*/
/*	while(Qvalptr = qget(CPUQueue))
	{
		apic = (struct APICLocalAPICEntry*)Qvalptr;
		kprintf("Found APIC, Length = %x Processor ID = %x APIC ID = %x\n",apic->Length,apic->ProcessorID,apic->APICID);
	}

	while(Qvalptr = qget(IOAPICQueue))
	{
		ioapic = (struct APICIOAPICEntry*)Qvalptr;
		kprintf("found IOAPIC, Length = %x ID = %x Reserv = %x at address = %x interrupt base = %x\n",ioapic->Length,ioapic->IOAPICId,ioapic->Reserved,ioapic->IOAPICAddress,ioapic->GlobalSystemInterruptBase);
	}

	while(Qvalptr = qget(InterruptOverRideQueue))
	{
		interruptoverride = (struct APICInterruptOverrideEntry*)Qvalptr;
		kprintf("found Interrupt Over Ride, length = %x\n",interruptoverride->Length);
	}*/
}

void ACPI_Parse_PCIE_CPLX(struct ACPIHeader *header)
{
  uint8_t *device_start;
  
  struct PCIe_config *PCIe_root_dev;
    
	PCIe_root_cplxQ = qopen();
	
	device_start = (uint8_t *)header + sizeof(struct PCIeHeader);
	
	while(device_start < (uint8_t*)header + (header->Length) )
	{
	  PCIe_root_dev = (struct PCIe_config *)device_start;
	  qput(PCIe_root_cplxQ, (void *)PCIe_root_dev);
#if DEBUG_ACPI	 
	  kprintf("PCIe root complex Base address 0x%x \n", PCIe_root_dev->baseaddr);
	  kprintf("PCIe root complex group num 0x%x \n", PCIe_root_dev->segment_group);
    kprintf("PCIe controls bus %d, to bus  %d \n", PCIe_root_dev->start_bus, PCIe_root_dev->end_bus);
#endif	  
	  device_start += sizeof(struct PCIe_config);
	}

return;
}

void ACPI_Parse_Entry(struct ACPIHeader *header)
{
	const char *apic = "APIC";
	const char *dmar = "DMAR";
	const char *hpet = "HPET";
	const char *mcfg = "MCFG";
	/*Debug if you want to know what tables have been found*/
#ifdef DEBUG_ACPI
	kprintf("signature = %s\n",header->Signature);
#endif
	if(kstrncmp(header->Signature,apic,kstrlen(apic))==0)
	{
		ACPI_Parse_MADT((struct ACPIMADT *)header);

	}

	else if(kstrncmp(header->Signature,dmar,kstrlen(dmar))==0)
	{
#ifdef HYPV
		ACPI_Parse_DMAR((struct ACPIDMAR *)header);
#endif
	}

  else if(kstrncmp(header->Signature,hpet,kstrlen(hpet))==0)
    {
      ACPI_Parse_HPET((struct ACPIHeader *)header);
    }

  else if(kstrncmp(header->Signature,mcfg,kstrlen(mcfg))==0)
  {
    ACPI_Parse_PCIE_CPLX(header);
  }
return;  
}

void Scan_ACPI()
{
	struct ACPIHeader *rsdt;
	uint32_t *entry_ptr,*end_rsdt,address,address_end; 
	int rc;

	
	/*PARSE RSDP*/
	/*previosuly completed by scan rsdp in kentry*/
	/*Debugging*/
#ifdef DEBUG_ACPI	
	kprintf("[ACPI] OEMID = %s\n[ACPI] Revision = %x\n[ACPI] RSDT = %x\n[ACPI] XSDT = %x\n",rsdpdesc.OEMID,rsdpdesc.Revision,rsdpdesc.RsdtAddress,rsdpdesc.XsdtAddress);
#endif
	
	/*kprintf("rsdpdesc.RsdtAddress = %x\n",rsdpdesc.RsdtAddress & 0xFFFFFFF000);*/
	rc = attach_page(rsdpdesc.RsdtAddress & 0xFFFFFFF000,rsdpdesc.RsdtAddress & 0xFFFFFFF000, KMEM_IO_FLAGS);
	if(rc !=0)
	{
		kprintf("[ACPI] can't identity map ACPI tables");
		while(1);
	}

	/*PARSE RSDT*/
	/*have to cast it to a uintptr_t then a structure*/
	rsdt = (struct ACPIHeader *)(uintptr_t)(rsdpdesc.RsdtAddress);
	/*Debugging*/
#ifdef DEBUG_ACPI 
	kprintf("[ACPI] rsdt signature = %s\n[ACPI] rsdt OEMID = %s\n",rsdt->Signature,rsdt->OEMID);
#endif

	entry_ptr = (uint32_t*)(rsdt+1);
	end_rsdt = (uint32_t*)(((uint8_t*)rsdt)+rsdt->Length);
	address = *entry_ptr; /*Start of ACPI tables*/
	/*	end of ACPI tables plus one additional page to cover the case
			where the last ACPI table length over runs the end of the page
			for example the dell 9020 DMAR table exists at 0xXXXXDFF0 and goes
			to 0xXXXXE000. This is only a problem for the last table as we are
			identity mapping from the start to the end. (RMD)
	*/
	address_end = *(end_rsdt-1)+0x2000;
#ifdef DEBUG_ACPI
	kprintf("address %x entryptr = %x end_rsdt %x address_end %x\n",address,entry_ptr,end_rsdt,address_end);
#endif

	/*	First loop we iterate by page size over the entire ACPI table */
	while(address < address_end)
	{
	
		/*Debug where are these tables in memory*/
		/*kprintf("Address = %x\n",address);*/
		/*Remember things need to be page alligned to work correctly*/
		/*The BIOS writers won't do it for you*/
		rc = attach_page(address & 0xFFFFFFF000,address & 0xFFFFFFF000, KMEM_IO_FLAGS);
		if(rc !=0)/*Halt if ACPI isn't found*/
		{
			kprintf("[ACPI] can't identity map ACPI tables");
			while(1);
		}
		address+=0x1000;
	}
	
	/* Second loop we parse the now mapped in ACPI table */
	while(entry_ptr < end_rsdt)
	{
		address = *entry_ptr;
#ifdef DEBUG_ACPI
		kprintf("entry_ptr %x address %x\n",entry_ptr,address);
#endif
		ACPI_Parse_Entry((struct ACPIHeader *)(uintptr_t)address);
		entry_ptr++;
	}
#ifdef DEBUG_ACPI
		kprintf("[ACPI] Number of cpus found = %x\n",smp_num_cpus);
#endif
}
