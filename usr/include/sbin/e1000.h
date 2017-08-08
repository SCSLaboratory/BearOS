/**
 * @file e1000.h
 *
 * @brief Device driver implementation declarations for the
 *        Intel Pro/1000 Gigabit Ethernet card(s).
 *
 * Parts of this code is based on the DragonflyBSD (FreeBSD)
 * implementation, and the fxp driver for Minix 3.
 *
 * @see http://svn.freebsd.org/viewvc/base/head/sys/dev/e1000/
 * @see fxp.c
 *
 * @author Niek Linnenbank <nieklinnenbank@gmail.com>
 * @date September 2009
 *
 */

#ifndef __E1000_H
#define __E1000_H


#include <sbin/syspid.h>
#include <sbin/e1000_hw.h>

#define E1000_PING 3	

typedef struct {
  int type;
} E1000_ping_req_t;

typedef struct {
  int type;
  int value;
} E1000_ping_resp_t;

/**
 * @name Constants.
 * @{
 */

/** Number of receive descriptors per card. */
#define E1000_RXDESC_NR 128 

/** Number of transmit descriptors per card. */
#define E1000_TXDESC_NR 128  

/** Size of each I/O buffer per descriptor. */
/*this number must correspond to the bits set in the 
 *control register for this   */
#define E1000_IOBUF_SIZE 2048


/**
 * @}
 */

/**
 * @name Status Flags.
 * @{
 */

/** Card has been detected on the PCI bus. */
#define E1000_DETECTED (1 << 0)

/** Card is enabled. */
#define E1000_ENABLED  (1 << 1)

/** Client has requested to receive packets. */
#define E1000_READING  (1 << 2)

/** Client has requested to write packets. */
#define E1000_WRITING  (1 << 3)

/** Received some packets on the card. */
#define E1000_RECEIVED (1 << 4)

/** Transmitted some packets on the card. */
#define E1000_TRANSMIT (1 << 5)

/**
 * @}
 */

/**
 * @name Macros.
 * @{
 */


/**
 * Read a byte from flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 */
#define E1000_READ_FLASH_REG(e,reg)		\
  *(uint32_t *) (((e)->flash) + (reg))

/**
 * Read a 16-bit word from flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 */
#define E1000_READ_FLASH_REG16(e,reg)		\
  *(uint16_t *) (((e)->flash) + (reg))

/**
 * Write a 16-bit word to flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 * @param value New value.
 */
#define E1000_WRITE_FLASH_REG(e,reg,value)		\
  *((uint32_t *) (((e)->flash) + (reg))) = (value)

/**
 * Write a 16-bit word to flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 * @param value New value.
 */
#define E1000_WRITE_FLASH_REG16(e,reg,value)		\
  *((uint16_t *) (((e)->flash) + (reg))) = (value)

/**
 * @}
 */

typedef struct ether_addr
{
  uint8_t ea_addr[6];
} ether_addr_t;

typedef struct e1000_stat
{
  unsigned int recvErr,      /* # receive errors */
    OVW,                /* # buffer overwrite warnings,
			   (packets arrive faster than
			   can be processed) */
    CRCerr,             /* # crc errors of read */
    err_align,           /* # frames not alligned (# bits
			    not a mutiple of 8) */
    missedP,            /* # packets missed due to too
			   slow packet processing */
    packetR,            /* # packets received */
    packetT,            /* # packets transmitted */
    transDef,           /* # transmission defered (there
			   was a transmission of an
			   other station in progress */
    collision,          /* # collissions */
    good_pktT;

}e1000_stat_t;


/**
 * @brief Describes the state of an Intel Pro/1000 card.
 */
typedef struct e1000
{
  char name[8];		  /**< String containing the device name. */    
  int status;			  /**< Describes the card's current state. */
  int irq;			  /**< Interrupt Request Vector. */
  int ifnumber; 
   
  int revision;		  /**< Hardware Revision Number. */
  uint8_t *regs;		  	  /**< Memory mapped hardware registers. */
  uint8_t *flash;		  /**< Optional flash memory. */
  uint64_t flash_base_addr;	  /**< Flash base address. */
  ether_addr_t address;	  /**< Ethernet MAC address. */
  uint16_t (*eeprom_read)(void *, int reg); /**< Function to read  
					       the EEPROM. */
  int eeprom_done_bit;	  /**< Offset of the EERD.DONE bit. */    
  int eeprom_addr_off;	  /**< Offset of the EERD.ADDR field. */

  e1000_rx_desc_t *rx_desc;	  /**< Receive Descriptor table. */

  uint64_t rx_desc_p;	  /**< Physical Receive Descriptor Address. */
  int rx_desc_count;		  /**< Number of Receive Descriptors. */
  uint8_t *rx_buffer;		  /**< Receive buffer returned by malloc(). */
  int rx_buffer_size;		  /**< Size of the receive buffer. */

  e1000_tx_desc_t *tx_desc;	  /**< Transmit Descriptor table. */

  uint64_t tx_desc_p;	  /**< Physical Transmit Descriptor Address. */
  int tx_desc_count;		  /**< Number of Transmit Descriptors. */
  uint8_t *tx_buffer;		  /**< Transmit buffer returned by malloc(). */
  int tx_buffer_size;		  /**< Size of the transmit buffer. */

  e1000_stat_t stats;
    
       
  int rx_tail;   /*current Tail pointer inside the descriptor ring */
   
}e1000_t;




#endif /* __E1000_H */
