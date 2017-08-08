#pragma once
/*******************************************************************************
 *
 * File: e1000d_sys.h
 *
 * Description: Interface between the driver and whatever is using the driver.
 *    In this case, it's the userland bced process.  This file provides a
 *    layer of abstraction for the device driver, so that actions like
 *    allocating DMA-able memory, or mapping in the MMIO region, can happen
 *    however they need to.  These actions (functions) are dependent on where
 *    in the system the driver is running (ie, in the hypervisor, kernel, or
 *    a user process).
 *
 ******************************************************************************/

#include <stdint.h>
#include <sbin/netd.h>
#include <sbin/syspid.h>

int e1000_hw_init(Ndrv_config_msg_t *msg, Ndrv_init_req_t *req );
void e1000_interrupt();
void e1000_getmac_addr(Ndrv_init_resp_t *resp);
void e1000_output_pkt(Pkt_msg_t *pkt );



