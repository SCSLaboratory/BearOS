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

/******************************************************************************
 *
 * File: ke1000.c
 *
 * Description: The minimal amount of kernel code necessary to support the
 *              e1000 userland driver.
 *
 *****************************************************************************/

#include <ke1000.h>
#include <constants.h>
#include <ktimer.h>
#include <pci.h>
#include <kmsg.h>
#include <msg.h>
#include <msg_types.h>
#include <kvmem.h>
#include <memory.h>
#include <kstdio.h>
#include <kstring.h>
#include <procman.h>
#include <interrupts.h>
#include <ksched.h>
#include <kernel.h>
#include <kvcall.h>
#include <syscall.h>	/* for SYS message */

#include <sbin/netd.h>	/* Message definitions for net daemon */

void ke1000_spawn_driver(Pci_dev_t *dev);
void ke1000_attempt_spawn(void *vdev); 
void ke1000_set_driver_perms(Proc_t *p);

/* Each driver gets 64 megs of uncached mem to use for DMA. */
#define KE1000_UDMA_PAGES (uint64_t)0x8a00

#ifdef KERNEL
/*finds all the cards in the system and start a driver process for each */
void ke1000_spawn_drivers() {
  pci_apply(&ke1000_attempt_spawn);
}
#endif

int ke1000_is_supported_card(Pci_dev_t *dev) {

  /* If it's not a intel e1000 card, give up */
  if (dev->vendor == PCI_VENDOR_INTEL &&
      (dev->dev_id == 0x100E || dev->dev_id ==0x107C || dev->dev_id ==0x10D3 || dev->dev_id == 0x100F || dev->dev_id == 0x153A)){
#ifdef NET_DEBUG			
    kprintf("[E1000] supported card found id %x \n", dev->dev_id);
#endif
    return 1;
  }		
  return 0;
}


#ifdef KERNEL
void ke1000_attempt_spawn(void *vdev) {
  Pci_dev_t *dev;
	
  dev = (Pci_dev_t *)vdev;

  if (ke1000_is_supported_card(dev))
    ke1000_spawn_driver(dev);
}
#endif

#ifdef KERNEL
void ke1000_spawn_driver(Pci_dev_t *dev) {
  Proc_t *p;

  /* Create driver proc */
  p = new_proc(USR_TEXT_START, PL_3, E1000D, NULL);
  //	kprintf("e1000d loaded with pid %d\n",p->pid);
  setprocname(p, "e1000d");
  /* TODO: this func should change parallel to how kload_daemon changed */
  //rc = load_elf_proc("e1000d", p, 0);

  if (p == NULL) {
    kprintf("ERROR: Could not load e1000d\n");
    return;
  }
  ksched_add(p);
  p->dev = dev;
  
  /* Add interrupt handler */
  /* FIXME: This assumes use of legacy interrupts */
  network_interrupt_mask = dev->interrupt_line + INTR_OFFSET;
  intr_add_handler(dev->interrupt_line + INTR_OFFSET, &interrupt, (void*)((uintptr_t)(p->pid)));
#ifdef NET_DEBUG
  kprintf("[E1000] interrupt line is %d \n", dev->interrupt_line + INTR_OFFSET);
#endif
  ke1000_set_driver_perms(p);
}
#endif

#ifdef KERNEL
/* 
 * Called by driver spawn function, and also called by driver 
 * refresh module. 
 */
void ke1000_set_driver_perms(Proc_t *p) {
  Message_t msg;
  Ndrv_config_msg_t config_msg;
	
  /* Copy PCI dev info into msg */
  kmemcpy(&(config_msg.dev), p->dev, sizeof(Pci_dev_t));
  config_msg.type = NDRV_CONFIG;
	
  /* system call: Send msg with memory config */
  msg.src = SYS;
  msg.dst = p->pid;
  msg.len = sizeof(Ndrv_config_msg_t);
  msg.status = NULL;
  msg.buf = &config_msg;
  kmsg_send(&msg);
   
}
#endif /* ifdef KERNEL */

