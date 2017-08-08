/*
  Sourced from MINIX, then modified for use in Bear.
*/
#include <msg.h>
#include <msg_types.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <syscall.h>	/* For unmask_irq */

#include <utils/queue.h>

#include <sys/socket.h>
#include <netdb.h>

#include <sbin/pci_structs.h>
#include <sbin/e1000d.h>
#include <sbin/dma.h>
#include <sbin/e1000.h>
#include <sbin/e1000_hw.h>
#include <sbin/e1000_reg.h>
#include <sbin/e1000_pci.h>



/* taken from lwip/etharp.h */
#define IPH_LEN(hdr) ((hdr)->_len)
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define ETHARP_HWADDR_LEN     6
#define ETH_PAD_SIZE          0
#define ETHTYPE_ARP       0x0806U
#define ETHTYPE_IP        0x0800U



struct ip_addr_packed {
  PACK_STRUCT_FIELD(uint32_t addr);
} PACK_STRUCT_STRUCT;

typedef struct ip_addr_packed ip_addr_p_t;

struct ip_hdr {
  /* version / header length / type of service */
  PACK_STRUCT_FIELD(uint16_t _v_hl_tos);
  /* total length */
  PACK_STRUCT_FIELD(uint16_t _len);
  /* identification */
  PACK_STRUCT_FIELD(uint16_t _id);
  /* fragment offset field */
  PACK_STRUCT_FIELD(uint16_t _offset);
#define IP_RF 0x8000U        /* reserved fragment flag */
#define IP_DF 0x4000U        /* dont fragment flag */
#define IP_MF 0x2000U        /* more fragments flag */
#define IP_OFFMASK 0x1fffU   /* mask for fragmenting bits */
  /* time to live */
  PACK_STRUCT_FIELD(uint8_t _ttl);
  /* protocol*/
  PACK_STRUCT_FIELD(uint8_t _proto);
  /* checksum */
  PACK_STRUCT_FIELD(uint16_t _chksum);
  /* source and destination IP addresses */
  PACK_STRUCT_FIELD(ip_addr_p_t src);
  PACK_STRUCT_FIELD(ip_addr_p_t dest); 
} PACK_STRUCT_STRUCT;


struct eth_addr {
  PACK_STRUCT_FIELD(uint8_t addr[ETHARP_HWADDR_LEN]);
} PACK_STRUCT_STRUCT;

/** Ethernet header */
struct eth_hdr {
#if ETH_PAD_SIZE
  PACK_STRUCT_FIELD(uint8_t padding[ETH_PAD_SIZE]);
#endif
  PACK_STRUCT_FIELD(struct eth_addr dest);
  PACK_STRUCT_FIELD(struct eth_addr src);
  PACK_STRUCT_FIELD(uint16_t type);
} PACK_STRUCT_STRUCT;



static e1000_t e1000_state;
e1000_t *e;
static Pci_dev_t e1000_dev;
static void e1000_linkinput(uint8_t *dbuf);
static uint16_t e1000_htons(uint16_t n);
static uint16_t e1000_ntohs(uint16_t n);
static void send_linkstatus(unsigned int ifnum, int status);
/*functions for controlling the card memory*/
static uint32_t e1000_reg_read(e1000_t *e, uint32_t reg);
static void e1000_reg_write(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_set(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_unset(e1000_t *e, uint32_t reg, uint32_t value);
static uint16_t eeprom_eerd(void *e, int reg);
static uint16_t eeprom_ich(void *e, int reg);
static int eeprom_ich_init(e1000_t *e);
static int eeprom_ich_cycle(const e1000_t *e, uint32_t timeout);
static void e1000_check_speed(e1000_t *e);
static void e1000_check_link(e1000_t *e);
/*configuring the hardware */
static void e1000_init_mac_addr(e1000_t *e);
static void e1000_reset_card(e1000_t *e);

/*statistics registers for debugging */
static void e1000_getstats(e1000_t *e);
static void e1000_printstats(e1000_t *e);

/*setting up the DMA memory spaces */
static int e1000_init_tx(e1000_t *e);
static int e1000_init_rx(e1000_t *e);

/*turn on and configure interrupts */
static void e1000_init_intr(e1000_t *e);

//static void e1000_tx_intr();
static void e1000_rx_intr();

/*for talking to the nic card phy interface */
int e1000_read_phy_reg(e1000_t *e, uint16_t phy_reg, uint16_t *phy_data);
int e1000_write_phy_reg(e1000_t *e, uint16_t phy_reg, uint16_t phy_data);
void print_phy(e1000_t *e);

#define PAGE_SIZE 4096

uint32_t cards = -1;

static void     *e1000_mmio_vaddr;
static void     *e1000_flash_vaddr;




int e1000_hw_init(Ndrv_config_msg_t *msg, Ndrv_init_req_t *req) {
	
  uint16_t did, cr, i; 
  uint32_t gfpreg, sector_base_addr;
  Msg_status_t status;

  Map_mmio_req_t mmio_req;
  Map_mmio_resp_t mmio_resp;

  Map_dma_req_t dma_req;
  Map_dma_resp_t dma_resp;

  /*get the memory info from the kernel */
  memcpy(&e1000_dev, &(msg->dev), sizeof(Pci_dev_t));

  /*setup message to get mmio mapped in */
  mmio_req.type = SC_MAP_MMIO;
  mmio_req.pci_dev = e1000_dev;

  /*map in the first pci Bar */
  
  mmio_req.tag = mmio_resp.tag = get_msg_tag();
  mmio_req.bar = 0;
  msgsend(SYS, &mmio_req, sizeof(Map_mmio_req_t));
  msgrecv(SYS, &mmio_resp, sizeof(Map_mmio_resp_t), &status);
  e1000_mmio_vaddr =  mmio_resp.virt_addr;

#ifdef NET_DEBUG
  printf("mmio regs at %p \n", e1000_mmio_vaddr);
#endif

  /*map in the second pci bar   (flash) */
  mmio_req.tag = mmio_resp.tag = get_msg_tag();
  mmio_req.bar = 1;
  msgsend(SYS, &mmio_req, sizeof(Map_mmio_req_t));
  msgrecv(SYS, &mmio_resp, sizeof(Map_mmio_resp_t), &status);
  e1000_flash_vaddr = mmio_resp.virt_addr;

  /* Clear state. */
  memset(&e1000_state, 0, sizeof(e1000_state));

  did = e1000_dev.dev_id;
  e = &e1000_state;
	
  e->ifnumber = req->ifnum;
  strncpy(e->name, "e1000#0", sizeof(e->name));
  e->name[6] += (int) e->ifnumber;	
  cards++;
	
  e->eeprom_read = eeprom_eerd;
  e->irq = e1000_dev.interrupt_line;

  /*
   * Set card specific properties.
   */
  switch (did)
    {
    case E1000_DEV_ID_ICH10_D_BM_LM:
    case E1000_DEV_ID_ICH10_R_BM_LF:
      e->eeprom_read = eeprom_ich;
      break;
    case E1000_DEV_ID_82540EM:
    case E1000_DEV_ID_82545EM:
      e->eeprom_done_bit = (1 << 4);
      e->eeprom_addr_off =  8;
      break;
    default:
      e->eeprom_done_bit = (1 << 1);
      e->eeprom_addr_off =  2;
      break;
    }
#ifdef NET_DEBUG
  printf("Test reading mmio registers \n"); 
#endif	
  e->regs  = e1000_mmio_vaddr;
  if (e->regs == (uint8_t *) -1) {
    printf("[%s]:Failed to map hardware registers from PCI", e->name);
    return 1;
  }
#ifdef NET_DEBUG
  printf("success reading mmio regs \n"); 
#endif	
	
  cr = e1000_dev.command;
	
  if (!(cr & PCI_CONTROL_DMA_MAST_EN))
    {
#ifdef NET_DEBUG	
      printf("Enabling busmastering DMA \n");
#endif
      /* Syscall to tell kernel to write PCI config */
      Pci_set_config_req_t req;
      Pci_set_config_resp_t resp;
      Msg_status_t status;

      memset(&req, 0, sizeof(Pci_set_config_req_t));
      req.type = SC_PCI_SET;
      req.pci_dev = e1000_dev;
      req.offset = 0x04;
      req.val = (cr | PCI_CONTROL_DMA_MAST_EN);
      req.width = 2;	

      msgsend(SYS, &req, sizeof(Pci_set_config_req_t));
      msgrecv(SYS, &resp, sizeof(Pci_set_config_resp_t), &status);

      if(resp.ret_val != 0)
	printf("[E1000] Warning failed to set PCI config\n");	

    }

  if (did != E1000_DEV_ID_82540EM && did != E1000_DEV_ID_82545EM &&
      did != E1000_DEV_ID_82540EP && e1000_dev.pci_dev_bars[1].bar)
    {
		
      e->flash = e1000_flash_vaddr;

      gfpreg = E1000_READ_FLASH_REG(e, ICH_FLASH_GFPREG);
	
      /*
       * sector_base_addr is a "sector"-aligned address (4096 bytes)
       */
      sector_base_addr = gfpreg & FLASH_GFPREG_BASE_MASK;
		
      /* flash_base_addr is byte-aligned */
      e->flash_base_addr = sector_base_addr << FLASH_SECTOR_ADDR_SHIFT;
    }

  e->status  |= E1000_ENABLED;
  
  e1000_reset_card(e);

  /*
   * Initialize appropriately, according to section 14.3 General Configuration
   * of Intel's Gigabit Ethernet Controllers Software Developer's Manual.
   */
  e1000_reg_set(e,   E1000_REG_CTRL, E1000_REG_CTRL_ASDE | E1000_REG_CTRL_SLU);
  e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_LRST);
  e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_PHY_RST);
  e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_ILOS);
  e1000_reg_write(e, E1000_REG_FCAL, 0);
  e1000_reg_write(e, E1000_REG_FCAH, 0);
  e1000_reg_write(e, E1000_REG_FCT,  0);
  e1000_reg_write(e, E1000_REG_FCTTV, 0);
  /*We're not messing with vlans yet */
  e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_VME);
  /*Disable power management */
#if 0	
  e1000_reg_set(e,   E1000_REG_ECTRL, E1000_REG_ECTRL_PHY_PWRMGT);
  e1000_reg_set(e, E1000_REG_WAKEUP_CTRL, E1000_REG_WAKEUP_CTRL_APME);
  e1000_reg_set(e, E1000_REG_WAKEUP_FILTER, E1000_REG_WAKEUP_FILTER_EX |
		E1000_REG_WAKEUP_FILTER_MC | E1000_REG_WAKEUP_FILTER_BC 
		| E1000_REG_WAKEUP_FILTER_ARP);	
#endif	
 

  /* Clear Multicast Table Array (MTA). */
  for (i = 0; i < 128; i++)
    {
      e1000_reg_write(e, E1000_REG_MTA + i, 0);
    }
  /* Initialize statistics registers. */
  for (i = 0; i < 64; i++)
    {
      e1000_reg_write(e, E1000_REG_CRCERRS + (i * 4), 0);
    }
  /*Aquire MAC address and */
 
  e1000_init_mac_addr(e);

  /*How fast are we? because it's nice to know */
  e1000_check_speed(e);
#ifdef NET_DEBUG
  printf("[E1000] Setting up TX/RX buffers \n");
#endif

  e1000_init_tx(e);
  e1000_init_rx(e);
		 
  /*Setup and enable interrupts.*/
  e1000_init_intr(e);
  
	/*enable_msi(unsigned char irq, int level, int dest, int mode, int trigger, int bus, int dev, int func  ) */
	//enable_msi(e1000_dev.interrupt_line + 0x20, 0, 0, 0, 1, e1000_dev.bus, e1000_dev.slot, e1000_dev.func  );
	
  unmask_irq(e1000_dev.interrupt_line);
  return 0;
}

/*===============================================================*
 *				e1000_check_speed	 						     *
 * Initiates a speed check so we know how fast the card is		 *	
 * Useful later if you want to slow it down then you just        *
 * have to set the control bits on the CTRL register, the	     *
 * bit numbers to set speed are already in the .h file           *
 *===============================================================*/
static void e1000_check_speed(e1000_t *e){
  uint32_t status;

  /*initiate a self check for speed, self clears */
  e1000_reg_set(e, E1000_REG_ECTRL, E1000_REG_ECTRL_ASDCHK );

  /*pull in the status */
  status = e1000_reg_read(e, E1000_REG_STATUS);

  /*filter out the speed bits (6 & 7) */
  status = (status & 0xc0) >> 6; 

#ifdef NET_DEBUG
  if(status == 0)
    printf("[%s]speed is 10Mb/s \n", e->name);
  if(status == 1)
    printf("[%s]speed is 100Mb/s \n", e->name);
  if(status == 2)
    printf("[%s]speed is 1000Mb/s \n", e->name);	
#endif
}

/*===============================================================*
 *				e1000_check_link		 						 *
 * Figures out if the link change interrupt was from up or down  *	
 *===============================================================*/
static void e1000_check_link(e1000_t *e){
  uint32_t status;
	
  status = e1000_reg_read(e, E1000_REG_STATUS);
  if(e->status  == E1000_ENABLED){
    if(status & E1000_REG_STATUS_LU){
#ifdef NET_DEBUG
      printf("[%s]:link is up\n", e->name);
#endif
      send_linkstatus(e->ifnumber,1);
    }
    else{
#ifdef NET_DEBUG
      printf("[%s]:link is down\n", e->name);
#endif
      send_linkstatus(e->ifnumber,0);
    }
  }	
}


	
/*===========================================================================*
 *				e1000_init_addr											     *
 * Set the MAC addres of the card, choice to pull from the flash or hard set *
 *===========================================================================*/
static void e1000_init_mac_addr(e1000_t *e){
  uint16_t word;
  int i;

  /*
   * You can hardcode the address here if you prefer
   * simply comment out the lines to pull the mac form the firmware
   */
#ifdef NET_DEBUG
  printf("card id is %x \n", e1000_dev.dev_id);
#endif 
  if(e1000_dev.dev_id == 0x153A){

    e->address.ea_addr[0]= 0x00;
    e->address.ea_addr[1]= 0x23;
    e->address.ea_addr[2]= 0xDE;
    e->address.ea_addr[3]= 0xAD;
    e->address.ea_addr[4]= 0xBE;
    e->address.ea_addr[5]= 0xEF;
  }
  else{
    /*Pull the mac from the rom */
    for (i = 0; i < 3; i++){
      word = e->eeprom_read(e, i);
      e->address.ea_addr[(i * 2)]     = (word & 0xff);
      e->address.ea_addr[(i * 2) + 1] = (word & 0xff00) >> 8;
    }
  }

  /*
   * Set Receive Address. You can set multiple later if you like
   * just have to mult the base reg address by 8 however that means
   * tinkering with the macros which i don't have a need for now. 
   */
  e1000_reg_write(e, E1000_REG_RAL, *(uint32_t *)(&e->address.ea_addr[0]));
  e1000_reg_write(e, E1000_REG_RAH, *(uint16_t *)(&e->address.ea_addr[4]));
  e1000_reg_set(e,   E1000_REG_RAH,   E1000_REG_RAH_AV);
  e1000_reg_set(e,   E1000_REG_RAH,   E1000_REG_RAH_ASEL_HI);
  e1000_reg_set(e,   E1000_REG_RAH,   E1000_REG_RAH_ASEL_LO);
  /* multicast promiscious mode enable */
  /* if you turn this off the card goes to sleep and cannot be woken up*/
  e1000_reg_set(e,   E1000_REG_RCTL,  E1000_REG_RCTL_MPE);

#ifdef NET_DEBUG	
  printf("\n[%s]: Ethernet Address %x:%x:%x:%x:%x:%x\n", e->name,
	 e->address.ea_addr[0], e->address.ea_addr[1],
	 e->address.ea_addr[2], e->address.ea_addr[3],
	 e->address.ea_addr[4], e->address.ea_addr[5]);

#endif

}

static int e1000_init_tx(e1000_t *e){
  uint64_t tx_buff_p, i;
  Map_dma_req_t dma_req;
  Map_dma_resp_t dma_resp;
  Msg_status_t status;

  /***************Descriptors **********************/

  e->tx_desc_count = E1000_TXDESC_NR;
  dma_req.type = SC_MAP_DMA;
  dma_req.tag = dma_resp.tag = get_msg_tag();
  dma_req.num_bytes = sizeof(e1000_tx_desc_t) * E1000_TXDESC_NR;

  msgsend(SYS, &dma_req, sizeof(Map_dma_req_t));
  msgrecv(SYS, &dma_resp, sizeof(Map_dma_resp_t), &status);
#ifdef NET_DEBUG
  printf("TX Desc dma vaddr %p paddr %p size %x\n", dma_resp.virt_addr, dma_resp.phys_addr, dma_req.num_bytes);
#endif	
  e->tx_desc   = (e1000_tx_desc_t *)dma_resp.virt_addr;

  e->tx_desc_p = (uint64_t )dma_resp.phys_addr;

  memset(e->tx_desc, 0, sizeof(e1000_tx_desc_t) * E1000_TXDESC_NR);


  /***************Buffers ***************************/
  dma_req.type = SC_MAP_DMA;
  dma_req.tag = dma_resp.tag = get_msg_tag();
  dma_req.num_bytes = E1000_TXDESC_NR * E1000_IOBUF_SIZE;

  msgsend(SYS, &dma_req, sizeof(Map_dma_req_t));
  msgrecv(SYS, &dma_resp, sizeof(Map_dma_resp_t), &status);
#ifdef NET_DEBUG
  printf("TX Buf dma vaddr %p paddr %p size %X\n", dma_resp.virt_addr, dma_resp.phys_addr, dma_req.num_bytes);
#endif
  e->tx_buffer = (uint8_t *)(dma_resp.virt_addr);
  tx_buff_p    = (uint64_t)dma_resp.phys_addr;
	
  /*Add a buffer to each descirptor  */
  for (i = 0; i < E1000_TXDESC_NR; i++)
    {
      e->tx_desc[i].buffer = tx_buff_p + (i * E1000_IOBUF_SIZE);
    }	

  /* Setup the transmit ring registers.  */
  e1000_reg_write(e, E1000_REG_TDBAL, e->tx_desc_p & 0xFFFFFFFF );
  e1000_reg_write(e, E1000_REG_TDBAH, e->tx_desc_p >> 32);
  e1000_reg_write(e, E1000_REG_TDLEN, e->tx_desc_count * sizeof(e1000_tx_desc_t));
 
  e1000_reg_write(e, E1000_REG_TDH,   0);
  e1000_reg_write(e, E1000_REG_TDT,   0);
  
  e1000_reg_write(e, E1000_REG_TCTL,  E1000_REG_TCTL_PSP);
  e1000_reg_set(  e, E1000_REG_TCTL,  E1000_REG_TCTL_CT);  //set the ct field
  e1000_reg_set(  e, E1000_REG_TCTL,  E1000_REG_TCTL_COLD);  //collission dist 
  e1000_reg_set(  e, E1000_REG_TCTL,  E1000_REG_TCTL_EN);

  e1000_reg_write(e, E1000_REG_TIPG, 10);

}

static int e1000_init_rx(e1000_t *e){
  uint64_t rx_buff_p, i;
  Map_dma_req_t dma_req;
  Map_dma_resp_t dma_resp;
  Msg_status_t status;

  /***************Descriptors **********************/

  e->rx_desc_count = E1000_RXDESC_NR; 
  dma_req.type = SC_MAP_DMA;
  dma_req.tag = dma_resp.tag = get_msg_tag();
  dma_req.num_bytes = sizeof(e1000_rx_desc_t) * E1000_RXDESC_NR;

  msgsend(SYS, &dma_req, sizeof(Map_dma_req_t));
  msgrecv(SYS, &dma_resp, sizeof(Map_dma_resp_t), &status);
#ifdef NET_DEBUG
  printf("RX dma vaddr %p paddr %p size %x\n", (void *)dma_resp.virt_addr, (void *)dma_resp.phys_addr, dma_req.num_bytes);
#endif
  e->rx_desc   = (e1000_rx_desc_t * )dma_resp.virt_addr;

  e->rx_desc_p = (uint64_t)dma_resp.phys_addr;

  memset(e->rx_desc, 0, sizeof(e1000_rx_desc_t) * E1000_RXDESC_NR);


  /***************Buffers ***************************/
  dma_req.type = SC_MAP_DMA;
  dma_req.tag = dma_resp.tag = get_msg_tag();
  dma_req.num_bytes = E1000_RXDESC_NR * E1000_IOBUF_SIZE;

  msgsend(SYS, &dma_req, sizeof(Map_dma_req_t));
  msgrecv(SYS, &dma_resp, sizeof(Map_dma_resp_t), &status);
#ifdef NET_DEBUG  
  printf("RX buf dma vaddr %p paddr %p size %x\n", dma_resp.virt_addr, dma_resp.phys_addr, dma_req.num_bytes);
#endif
  e->rx_buffer = (uint8_t *)dma_resp.virt_addr;
  rx_buff_p    = (uint64_t)dma_resp.phys_addr;

  /*Add a buffer to each descirptor  */
  for (i = 0; i < E1000_RXDESC_NR; i++)
    {
      e->rx_desc[i].buffer = rx_buff_p + (i * E1000_IOBUF_SIZE);
    }
	
  e->rx_tail = 0;		

  /* Setup the receive ring registers.  */
  e1000_reg_write(e, E1000_REG_MTA,   0);
  e1000_reg_write(e, E1000_REG_RDBAH, e->rx_desc_p >> 32 );
  e1000_reg_write(e, E1000_REG_RDBAL, e->rx_desc_p & 0xFFFFFFFF );
  
  e1000_reg_write(e, E1000_REG_RDLEN, (e->rx_desc_count) *	sizeof(e1000_rx_desc_t));
  e1000_reg_write(e, E1000_REG_RDH,   0); /*todo set to one beyond last valid */
  e1000_reg_write(e, E1000_REG_RDT,   e->rx_desc_count -1);
  e1000_reg_unset(e, E1000_REG_RCTL,  E1000_REG_RCTL_BSIZE);
  e1000_reg_set(e,   E1000_REG_RCTL,  E1000_REG_RCTL_EN);

}




/*===========================================================================*
 *				e1000_reset_card										     *
 *===========================================================================*/
static void e1000_reset_card(e1000_t *e)
{
  /* Assert a Device Reset signal. */
  e1000_reg_set(e, E1000_REG_CTRL, E1000_REG_CTRL_RST);
  usleep(100);
}

/*===========================================================================*
 *				e1000_init_intr  										     *
 * This turns on and selects the options you want to recieve interrupts from *
 * the card for. We only need receive interrupts however if you need tx		 *
 * I left the options there so you can enable them they're only used if you  *
 * are sending enough that you're watching the tx queue and could fill it up *
 * TODO if you reinit the card you have to clear and reset the interrupts    *
 *==========================================================================*/
static void e1000_init_intr(e1000_t *e){

  /* Enable interrupts. */
  e1000_reg_write(e,   E1000_REG_IMS, E1000_REG_IMS_LSC   |
		  E1000_REG_IMS_SRPD |
		  E1000_REG_IMS_RXT |   
		  E1000_REG_IMS_RXO);/* |
					E1000_REG_IMS_TXDW |
					E1000_REG_IMS_TXQE);*/
	
  //	e1000_reg_set(e, E1000_REG_RADV,  100); //reeive abs delay
	
  //	e1000_reg_set(e, E1000_REG_RDTR, 0); //receive delay timer
	
  //e1000_reg_set(e, E1000_REG_ITR, 0xffff); 
	
  e1000_reg_write(e, E1000_REG_IMCR, 0xFFF60000 | E1000_REG_ICR_RXT | E1000_REG_IMS_TXQE	);
										 
  e1000_reg_set(e, E1000_REG_RSRPD, 0x1ff );
										 
}

/*===========================================================================*
 *				e1000_interrupt											     *
 *Main entry point for interrupts, called by the driver main loop when the   *
 *kernel sends us an interrupt message. 
 *===========================================================================*/
void e1000_interrupt(){
  uint32_t cause;

  /*
   * Check the card for interrupt reason(s).
   */

  /* Read the Interrupt Cause Read register. */
  if((cause = e1000_reg_read(e, E1000_REG_ICR))){
    //printf("cause is %x \n", cause);
    if(cause & ( E1000_REG_ICR_RXT)){
      //printf("read timer Interrupt \n");	
      e1000_rx_intr();
      //e1000_reg_write(e, E1000_REG_ICR, 0);
      unmask_irq(e1000_dev.interrupt_line);
      return;
				
    }
    else if (cause & 0x80000000){
      unmask_irq(e1000_dev.interrupt_line);
      return;
    }	
    else if(cause & E1000_REG_ICR_LSC){
      //printf("link status change intr \n");
      e1000_check_link(e);
    }
    else if(cause & (E1000_REG_ICR_RXO )){
      printf("read overrun Interrupt \n");	
    }
    else if(cause & ( E1000_REG_ICR_RXSEQ)){
      printf("read sequence error \n");
    }
    else if(cause & ( E1000_REG_ICR_RXDMT)){
      printf("warning running out of descriptors\n");
    }
    else if(cause & ( E1000_REG_ICR_SRPD)){
      printf("small packed received \n ");
      e1000_rx_intr();
    }
    else if(cause & ( E1000_REG_ICR_PHYINT)){
      printf(" phyint occurred \n");
    }
    else if(cause & ( E1000_REG_ICR_TXDW)){
      printf("completed transmits \n");
    }
    else if(cause & ( E1000_REG_ICR_TXQE)){
      //printf("transmit queue empty \n");
    }	
    else{
      printf("unknown Interrupt %d \n", cause);
    }
  }
  /*re-enable interrupts after servicing the previous one */
	
  unmask_irq(e1000_dev.interrupt_line);
}


/*===========================================================================*
 *			e1000_output_pkt				                                 *
 *===========================================================================*/
void e1000_output_pkt(Pkt_msg_t *pkt ){
 
  e1000_tx_desc_t *desc;
  uint64_t  tail;

  /* Find the head, tail and current descriptors. */
  tail =  e1000_reg_read(e, E1000_REG_TDT);

  desc = &e->tx_desc[tail];
	
  memcpy(e->tx_buffer + (tail * E1000_IOBUF_SIZE),
	 &(pkt->pkt),
	 pkt->len);	
	
#if 0
  kprintaddr_req_t printreq;
  printreq.type = SC_PRINT_ADDR;
  printreq.tag  = get_msg_tag();
  printreq.addr = e->tx_buffer + (tail * E1000_IOBUF_SIZE);
  printreq.bytes = 340;

  msgsend(SYS, &printreq, sizeof(kprintaddr_req_t));

  e1000_getstats(e);
  e1000_printstats(e);

  printf("tx addr %p  cur tail %d, pktlen %d\n", (uint64_t*)(e->tx_buffer + (tail * E1000_IOBUF_SIZE)), tail, pkt->len );
  for(z = 0, i=1; z < pkt->len; z++, i++){
    /*derefreence the buffer and print */
    printf("%x  ", *(uint8_t *)(e->tx_buffer + (tail * E1000_IOBUF_SIZE) + z ) );
    /*mmio keeps a copy of the first few packets see if this matches */
    printf("%x", *(uint8_t *)(e->regs + 0x1c000 + z ) );
    if(!(i%20))	
      printf("\n");
  }	
#endif 

  /* Mark this descriptor ready. */
  desc->status  = 0;
  desc->length  = pkt->len;
  desc->command = E1000_TX_CMD_EOP  |
    E1000_TX_CMD_FCS; /* |
			 E1000_TX_CMD_RS;*/
	
  /* Move to next descriptor. */
  tail   = (tail + 1) % e->tx_desc_count;
	
  /* Increment tail. Start transmission. */
  e1000_reg_write(e, E1000_REG_TDT,  tail);
	

}


/*===========================================================================*
 *				e1000_tx_intr            								     *
 *Used to handle Tx interrupts that we currently don't enable as theyre not  *
 * needed at the moment. However if you do here you go.                      *
 *===========================================================================*/
#if 0
static void e1000_tx_intr(){

  e1000_t *e = &e1000_state;
  e->status |= E1000_TRANSMIT;
}
#endif


/*===========================================================================*
 *				e1000_read_from interupt								     *
 *===========================================================================*/
static void e1000_rx_intr(){
  int tail, drop=0;
	
  while( (e->rx_desc[e->rx_tail].status) & (1 << 0) ){
    if(e->rx_desc[e->rx_tail].errors){
      printf("warning packet errors \n");
      drop = 1; 
    }
	
    if(drop	== 0){
      e1000_linkinput(e->rx_buffer + ((e->rx_tail) * E1000_IOBUF_SIZE));
    }
		
    e->rx_desc[e->rx_tail].status = (uint16_t)0;
    e->rx_tail = (e->rx_tail + 1) %e->rx_desc_count;
    e1000_reg_write(e, E1000_REG_RDT, e->rx_tail);
  }
}


/*============================================================================*
 *				e1000_linkinput		 						     			  *
 * Called by the rx interrupt routine to send a packe up to lwip			  *	
 *===========================================================================*/
static void e1000_linkinput(uint8_t *dbuf) {
  struct eth_hdr *ethhdr;
  Pkt_msg_t pkt;
  struct ip_hdr *iphdr;
  uint16_t ip_total_length, eth_hdr_len;
			
  /* Examine the packet and see if this is something we're interested in. */
  ethhdr = (struct eth_hdr *)dbuf;
	
  switch( e1000_htons(ethhdr->type) ) 
    {
    case ETHTYPE_ARP:
      /* APR packets are 42 bytes so kick that up*/
      /*also this prevents some idiot from using it as a covert comm link */
      pkt.len = 42;
      pkt.type = NET_PKT_INCOMING;  
      pkt.ifnum = e->ifnumber;

      memcpy(pkt.pkt, dbuf, pkt.len);
      msgsend(NETD, &pkt, sizeof(Pkt_msg_t) - 1600 + pkt.len);
      break;

    case ETHTYPE_IP:
      /*we need to get the size of the ip packet out of the header */
      eth_hdr_len = 	sizeof(struct eth_hdr);
      iphdr = (struct ip_hdr*)(dbuf+eth_hdr_len);
      ip_total_length = e1000_ntohs(IPH_LEN(iphdr));

      pkt.len = ip_total_length + eth_hdr_len;
      if(pkt.len > 1600){
	printf(" got to big of a packet");
	break;
      }
      pkt.type = NET_PKT_INCOMING;
      pkt.ifnum = e->ifnumber;
		
      memcpy(pkt.pkt, dbuf, pkt.len);
      msgsend(NETD, &pkt, sizeof(Pkt_msg_t) - 1600 + pkt.len);
      break;	
	
    default:
      return;
    } 
}

/*============================================================================*
 *				send_linkstatus 	 						     			  *
 * Informs lwip of card link status changes									  *	
 *===========================================================================*/

static void send_linkstatus(unsigned int ifnum, int status) {
  Link_status_msg_t stat;

  stat.type = NET_IFLINK;
  stat.ifnum = ifnum;
  stat.status = status;

  msgsend(NETD, &stat, sizeof(Link_status_msg_t));
}

/*===========================================================================*
 *				e1000_getmac											     *
 *===========================================================================*/
void e1000_getmac_addr(Ndrv_init_resp_t *resp) {
			
  resp->eaddr[0] = e1000_state.address.ea_addr[0];
  resp->eaddr[1] = e1000_state.address.ea_addr[1];
  resp->eaddr[2] = e1000_state.address.ea_addr[2];
  resp->eaddr[3] = e1000_state.address.ea_addr[3];
  resp->eaddr[4] = e1000_state.address.ea_addr[4];
  resp->eaddr[5] = e1000_state.address.ea_addr[5];

  resp->ifnum = e->ifnumber;

}

/*===========================================================================*
 *				e1000_getstate											     *
 *===========================================================================*/

static void e1000_getstats(e1000_t *e){

  e->stats.recvErr   += e1000_reg_read(e, E1000_REG_RXERRC);
  e->stats.OVW       = 0;
  e->stats.CRCerr    += e1000_reg_read(e, E1000_REG_CRCERRS);
  e->stats.err_align += e1000_reg_read(e,0x4004);
  e->stats.missedP   += e1000_reg_read(e, E1000_REG_MPC);
  e->stats.packetR   += e1000_reg_read(e, E1000_REG_TPR);
  e->stats.packetT   += e1000_reg_read(e, E1000_REG_TPT);
  e->stats.collision += e1000_reg_read(e, E1000_REG_COLC);
  e->stats.good_pktT += e1000_reg_read(e, 0x4080); 
	
}

static void e1000_printstats(e1000_t *e){

  printf("pktT %d, pct_GT %d, alignE %d, CRCe %d \n", e->stats.packetT, e->stats.good_pktT, e->stats.err_align, e->stats.CRCerr );

}
/* Utility function for endian byteswapping */
static uint16_t e1000_htons(uint16_t n) {
  return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

/* Utility function for endian byteswapping */
static uint16_t e1000_ntohs(uint16_t n) {
  return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

int e1000_read_phy_reg(e1000_t *e, uint16_t phy_reg, uint16_t *phy_data){

  uint32_t phy_command=0;
  uint32_t phy_temp_data;
  /*clear the error bit */
  phy_command |= (0<<30);
  /*don't use interrupts */
  phy_command |= (0<<29);
  /*clear the ready bit */
  phy_command |= (0<<28);
  /*config as read command */
  phy_command |= (1<<27);
  phy_command |= (0<<26);

  /*set the phy address per intel manual */
  phy_command |= (0<<25);
  phy_command |= (0<<24);
  phy_command |= (0<<23);
  phy_command |= (0<<22);
  phy_command |= (1<<21);
  /*shift in the register to read */
  phy_command |= (phy_reg << 16);
	
  e1000_reg_write(e, 0x20, phy_command);

  do{
    phy_temp_data = e1000_reg_read(e, 0x20);
  }while(! (phy_temp_data & (1 << 28)) ) ;
	
  if(phy_temp_data & (1<<30))
    return 1;	//error
  else{
    *phy_data = (uint16_t)phy_temp_data;
    return 0;
  }
}

void print_phy(e1000_t *e){
  uint16_t pctrl, pautoneg, pstatus, pinte, pints;
  int rc; 

  rc = e1000_read_phy_reg(e, 4, &pautoneg);
  rc = e1000_read_phy_reg(e, 0, &pctrl);  // PHY Control Register
  rc = e1000_read_phy_reg(e, 1, &pstatus);  // PHY Status Register
  rc = e1000_read_phy_reg(e, 1, &pstatus);  // PHY Status Register (read twice to get current link active value)
  rc = e1000_read_phy_reg(e, 18, &pinte);
  rc = e1000_read_phy_reg(e, 19, &pints);

  printf("=========================================================================\n");
       
  printf("PCTRL = 0x%04x\n", pctrl);
  printf("-------------------------\n");
  printf("   .reserved            = 0x%04x\n", (pctrl & 0x3F));
  printf("   .speed_selection_msb = %d\n", ((pctrl >> 6) & 0x1));
  printf("   .collision_test      = %d\n", ((pctrl >> 7) & 0x1));
  printf("   .duplex_mode         = %d\n", ((pctrl >> 8) & 0x1));
  printf("   .restart_autoneg     = %d\n", ((pctrl >> 9) & 0x1));
  printf("   .isolate             = %d\n", ((pctrl >> 10) & 0x1));
  printf("   .power_down          = %d\n", ((pctrl >> 11) & 0x1));
  printf("   .autoneg_enable      = %d\n", ((pctrl >> 12) & 0x1));
  printf("   .speed_selection_lsb = %d\n", ((pctrl >> 13) & 0x1));
  printf("   .loopback            = %d\n", ((pctrl >> 14) & 0x1));
  printf("   .reset               = %d\n", ((pctrl >> 15) & 0x1));

  printf("AUTONEG = 0x%04x\n", pautoneg);
  printf("-------------------------\n");
  printf("    .selector_field     = %d\n", (pautoneg & 0x1F));
  printf("    .10base-tx_half     = %d\n", ((pautoneg >> 5)& 0x1));
  printf("    .10base-tx_full     = %d\n", ((pautoneg >> 6)& 0x1));
  printf("    .100base-tx_half    = %d\n", ((pautoneg >> 7)& 0x1));
  printf("    .100base-tx_full    = %d\n", ((pautoneg >> 8)& 0x1));
  printf("    .100base-t4         = %d\n", ((pautoneg >> 9)& 0x1));
  printf("    .pause              = %d\n", ((pautoneg >> 10)& 0x1));
  printf("    .asymmetric_pause   = %d\n", ((pautoneg >> 11)& 0x1));
  printf("    .rsvd               = %d\n", ((pautoneg >> 12)& 0x1));
  printf("    .remote_fault       = %d\n", ((pautoneg >> 13)& 0x1));
  printf("    .rsvd               = %d\n", ((pautoneg >> 14)& 0x1));
  printf("    .next_page          = %d\n", ((pautoneg >> 15)& 0x1));

  printf("PSTATUS = 0x%04x\n", pstatus);
  printf("-------------------------\n");
  printf("   .extended_cap        = %d\n", ((pstatus >> 0) & 0x1));
  printf("   .jabber_detect       = %d\n", ((pstatus >> 1) & 0x1));
  printf("   .link_status         = %d\n", ((pstatus >> 2) & 0x1));
  printf("   .autoneg_ability     = %d\n", ((pstatus >> 3) & 0x1));
  printf("   .remote_fault        = %d\n", ((pstatus >> 4) & 0x1));
  printf("   .autoneg_complete    = %d\n", ((pstatus >> 5) & 0x1));
  printf("   .mf_preamble_supp    = %d\n", ((pstatus >> 6) & 0x1));
  printf("   .reserved            = %d\n", ((pstatus >> 7) & 0x1));
  printf("   .extended_status     = %d\n", ((pstatus >> 8) & 0x1));
  printf("   .100base_t2_half     = %d\n", ((pstatus >> 9) & 0x1));
  printf("   .100base_t2_full     = %d\n", ((pstatus >> 10) & 0x1));
  printf("   .10mb_half           = %d\n", ((pstatus >> 11) & 0x1));
  printf("   .10mb_full           = %d\n", ((pstatus >> 12) & 0x1));
  printf("   .100base_x_half      = %d\n", ((pstatus >> 13) & 0x1));
  printf("   .100base_x_full      = %d\n", ((pstatus >> 14) & 0x1));
  printf("   .100base_t4          = %d\n", ((pstatus >> 15) & 0x1));


  printf("PINTE = 0x%04x\n", pinte);
  printf("-------------------------\n");
  printf("   .jabber              = %d\n", ((pinte >> 0) & 0x1));
  printf("   .polarity_changed    = %d\n", ((pinte >> 1) & 0x1));
  printf("   .reserved            = 0x%01x\n", ((pinte >> 2) & 0x3));
  printf("   .energy_detect       = %d\n", ((pinte >> 4) & 0x1));
  printf("   .downshift           = %d\n", ((pinte >> 5) & 0x1));
  printf("   .mdi_crossover       = %d\n", ((pinte >> 6) & 0x1));
  printf("   .fifo_overunflow     = %d\n", ((pinte >> 7) & 0x1));
  printf("   .false_carrier       = %d\n", ((pinte >> 8) & 0x1));
  printf("   .symbol_error        = %d\n", ((pinte >> 9) & 0x1));
  printf("   .link_status_change  = %d\n", ((pinte >> 10) & 0x1));
  printf("   .autoneg_complete    = %d\n", ((pinte >> 11) & 0x1));
  printf("   .page_received       = %d\n", ((pinte >> 12) & 0x1));
  printf("   .duplex_changed      = %d\n", ((pinte >> 13) & 0x1));
  printf("   .speed_changed       = %d\n", ((pinte >> 14) & 0x1));
  printf("   .autoneg_error       = %d\n", ((pinte >> 15) & 0x1));

  printf("PINTS = 0x%04x\n", pints);
  printf("-------------------------\n");
  printf("   .jabber              = %d\n", ((pints >> 0) & 0x1));
  printf("   .polarity_changed    = %d\n", ((pints >> 1) & 0x1));
  printf("   .reserved            = 0x%01x\n", ((pints >> 2) & 0x3));
  printf("   .energy_detect       = %d\n", ((pints >> 4) & 0x1));
  printf("   .downshift           = %d\n", ((pints >> 5) & 0x1));
  printf("   .mdi_crossover       = %d\n", ((pints >> 6) & 0x1));
  printf("   .fifo_overunflow     = %d\n", ((pints >> 7) & 0x1));
  printf("   .false_carrier       = %d\n", ((pints >> 8) & 0x1));
  printf("   .symbol_error        = %d\n", ((pints >> 9) & 0x1));
  printf("   .link_status_change  = %d\n", ((pints >> 10) & 0x1));
  printf("   .autoneg_complete    = %d\n", ((pints >> 11) & 0x1));
  printf("   .page_received       = %d\n", ((pints >> 12) & 0x1));
  printf("   .duplex_changed      = %d\n", ((pints >> 13) & 0x1));
  printf("   .speed_changed       = %d\n", ((pints >> 14) & 0x1));
  printf("   .autoneg_error       = %d\n", ((pints >> 15) & 0x1));

  printf("=========================================================================\n");
}


int e1000_write_phy_reg(e1000_t *e, uint16_t phy_reg, uint16_t phy_data){

  uint32_t phy_command=0;
  uint32_t phy_temp_data;
  /*clear the error bit */
  phy_command |= (0<<30);
  /*don't use interrupts */
  phy_command |= (0<<29);
  /*clear the ready bit */
  phy_command |= (0<<28);
  /*config as read command */
  phy_command |= (0<<27);
  phy_command |= (1<<26);

  /*set the phy address per intel manual */
  phy_command |= (0<<25);
  phy_command |= (0<<24);
  phy_command |= (0<<23);
  phy_command |= (0<<22);
  phy_command |= (1<<21);
  /*shift in the register to read */
  phy_command |= (phy_reg << 16);
	
  phy_command |= phy_data;	

  e1000_reg_write(e, 0x20, phy_command);

  do{
    phy_temp_data = e1000_reg_read(e, 0x20);
  }while(! (phy_temp_data & (1 << 28)) ) ;
	
  if(phy_temp_data & (1<<30))
    return 1;	//error
  else{
    return 0;
  }
}

/*===========================================================================*
 *				e1000_reg_read				     *
 *===========================================================================*/
static uint32_t e1000_reg_read(e1000_t *e, uint32_t reg)
{
  uint64_t value;

  /* Assume a sane register. */
  // assert(reg < 0x1ffff);

  /* Read from memory mapped register. */
  // printf("reading regiser %p \n", (e->regs + reg));
    
  value = *(volatile uint32_t *)(e->regs + reg);

  /* Return the result. */    
  return value;
}

/*===========================================================================*
 *				e1000_reg_write				     *
 *===========================================================================*/
static void e1000_reg_write(e1000_t *e, uint32_t reg, uint32_t value)
{
  /* Write to memory mapped register. */
  *(volatile uint32_t *)(e->regs + reg) = value;
}

/*===========================================================================*
 *				e1000_reg_set				     *
 *===========================================================================*/
static void e1000_reg_set(e1000_t *e, uint32_t reg, uint32_t value)

{
  uint64_t data;

  /* First read the current value. */
  data = e1000_reg_read(e, reg);

  /* Set value, and write back. */
  e1000_reg_write(e, reg, data | value);
}

/*===========================================================================*
 *				e1000_reg_unset				     *
 *===========================================================================*/
static void e1000_reg_unset(e1000_t *e, uint32_t reg, uint32_t value)
{
  uint64_t data;

  /* First read the current value. */
  data = e1000_reg_read(e, reg);
    
  /* Unset value, and write back. */
  e1000_reg_write(e, reg, data & ~value);
}


/*===========================================================================*
 *				eeprom_eerd				     *
 *===========================================================================*/
static uint16_t eeprom_eerd(v, reg)
     void *v;
     int reg;
{
  e1000_t *le = (e1000_t *) v;
  uint32_t data;

  /* Request EEPROM read. */
  e1000_reg_write(le, E1000_REG_EERD,
		  (reg << le->eeprom_addr_off) | (E1000_REG_EERD_START));

  /* Wait until ready. */
  while (!((data = (e1000_reg_read(le, E1000_REG_EERD))) & le->eeprom_done_bit));

  return data >> 16;
}

/*===========================================================================* 
 *                              eeprom_ich_init                              * 
 *===========================================================================*/
static int eeprom_ich_init(e)
     e1000_t *e;
{
  union ich8_hws_flash_status hsfsts;
  int ret_val = -1;
  int i = 0;

  hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);

  /* Check if the flash descriptor is valid */
  if (hsfsts.hsf_status.fldesvalid == 0)
    {
      printf("[%s]:Flash descriptor invalid. SW Sequencing must be used."
	     , e->name);
      goto out;                                                         
    }                                                                         
  /* Clear FCERR and DAEL in hw status by writing 1 */                      
  hsfsts.hsf_status.flcerr = 1;                                             
  hsfsts.hsf_status.dael   = 1;                                     
                                                                        
  E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);   
                                                                        
  /*                                                              
   * Either we should have a hardware SPI cycle in progress       
   * bit to check against, in order to start a new cycle or       
   * FDONE bit should be changed in the hardware so that it 
   * is 1 after hardware reset, which can then be used as an 
   * indication whether a cycle is in progress or has been 
   * completed. 
   */                                                                
  if (hsfsts.hsf_status.flcinprog == 0)
    {
      /* 
       * There is no cycle running at present, 
       * so we can start a cycle. 
       * Begin by setting Flash Cycle Done. 
       */
      hsfsts.hsf_status.flcdone = 1;
      E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);
      ret_val = 0;
    }
  else
    {
      /* 
       * Otherwise poll for sometime so the current 
       * cycle has a chance to end before giving up. 
       */
      for (i = 0; i < ICH_FLASH_READ_COMMAND_TIMEOUT; i++)
	{
	  hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
            
	  if (hsfsts.hsf_status.flcinprog == 0)
	    {
	      ret_val = 0;
	      break;
            }
	  // tickdelay(1);
        }
      if (ret_val == 0)
	{
	  /* 
	   * Successful in waiting for previous cycle to timeout, 
	   * now set the Flash Cycle Done. 
	   */
	  hsfsts.hsf_status.flcdone = 1;
	  E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS,
				  hsfsts.regval);
        }
      else
	{
	  printf("[%s]:Flash controller busy, cannot get access", e->name);
        }
    }
 out:    
  return ret_val;
}

/*===========================================================================* 
 *                              eeprom_ich_cycle                             * 
 *===========================================================================*/
static int eeprom_ich_cycle(const e1000_t *e, uint32_t timeout)
{
  union ich8_hws_flash_ctrl hsflctl;
  union ich8_hws_flash_status hsfsts;
  int ret_val = -1;
  uint32_t i = 0;

  /* Start a cycle by writing 1 in Flash Cycle Go in Hw Flash Control */
  hsflctl.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFCTL);
  hsflctl.hsf_ctrl.flcgo = 1;
  E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFCTL, hsflctl.regval);
                
  /* wait till FDONE bit is set to 1 */
  do
    {
      hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
      if (hsfsts.hsf_status.flcdone == 1)
	break;
      /*FIXME*/        
      //tickdelay(1);
    }
  while (i++ < timeout);
                                                              
  if (hsfsts.hsf_status.flcdone == 1 && hsfsts.hsf_status.flcerr == 0)
    ret_val = 0;
                                
  return ret_val;
}

/*=====================================================*
 *			eeprom_ich				     *
 *=======================================================*/
static uint16_t eeprom_ich(v, reg)
     void *v;
     int reg;
{
  union ich8_hws_flash_status hsfsts;
  union ich8_hws_flash_ctrl hsflctl;
  uint32_t flash_linear_addr;
  uint32_t flash_data = 0;
  int ret_val = -1;
  uint8_t count = 0;
  e1000_t *le = (e1000_t *) v;
  uint16_t data = 0;
                        
                                      
  if (reg > ICH_FLASH_LINEAR_ADDR_MASK)
    goto out;

  reg *= sizeof(uint16_t);                
  flash_linear_addr = (ICH_FLASH_LINEAR_ADDR_MASK & reg) +
    le->flash_base_addr;

  do {
    /*FIXME*/	
    //tickdelay(1);
        
    /* Steps */
    ret_val = eeprom_ich_init(le);
    if (ret_val != 0)
      break;

    hsflctl.regval = E1000_READ_FLASH_REG16(le, ICH_FLASH_HSFCTL);
    /* 0b/1b corresponds to 1 or 2 byte size, respectively. */
    hsflctl.hsf_ctrl.fldbcount = 1;
    hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_READ;
    E1000_WRITE_FLASH_REG16(le, ICH_FLASH_HSFCTL, hsflctl.regval);
    E1000_WRITE_FLASH_REG(le, ICH_FLASH_FADDR, flash_linear_addr);

    ret_val = eeprom_ich_cycle(v, ICH_FLASH_READ_COMMAND_TIMEOUT);
        
    /* 
     * Check if FCERR is set to 1, if set to 1, clear it 
     * and try the whole sequence a few more times, else 
     * read in (shift in) the Flash Data0, the order is 
     * least significant byte first msb to lsb 
     */
    if (ret_val == 0)
      {
	flash_data = E1000_READ_FLASH_REG(le, ICH_FLASH_FDATA0);
	data = (uint16_t)(flash_data & 0x0000FFFF);
	break;
      }
    else
      {
	/* 
	 * If we've gotten here, then things are probably 
	 * completely hosed, but if the error condition is 
	 * detected, it won't hurt to give it another try... 
	 * ICH_FLASH_CYCLE_REPEAT_COUNT times. 
	 */
	hsfsts.regval = E1000_READ_FLASH_REG16(le, ICH_FLASH_HSFSTS);
            
	if (hsfsts.hsf_status.flcerr == 1)
	  {
	    /* Repeat for some time before giving up. */
	    continue;
	  }
	else if (hsfsts.hsf_status.flcdone == 0)
	  {
	    printf("[%s]:Timeout error - flash cycle, did not complete.", 
		   le->name);
	    break;
	  }
      }
  } while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

 out:
  return data;
}


