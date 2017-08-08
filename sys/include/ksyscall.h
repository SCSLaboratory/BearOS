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

#include <kmsg.h>
#include <msg.h>
#include <syscall.h>		/* in usr/include */

/* SYSCALL SERVICE ROUTINES */
void systask_do_fork          (Systask_msg_t*, Msg_status_t*);
void systask_do_exec          (Systask_msg_t*, Msg_status_t*);
void systask_do_getpid        (Systask_msg_t*, Msg_status_t*);
void systask_do_waitpid       (Systask_msg_t*, Msg_status_t*);
void systask_do_exit          (Systask_msg_t*, Msg_status_t*);
void systask_do_kill          (Systask_msg_t*, Msg_status_t*);
void systask_do_spawn         (Systask_msg_t*, Msg_status_t*);
void systask_do_usleep        (Systask_msg_t*, Msg_status_t*);
void systask_do_map_vga_mem   (Systask_msg_t*, Msg_status_t*);
void systask_do_reboot        (Systask_msg_t*, Msg_status_t*);
void systask_do_unmask_irq    (Systask_msg_t*, Msg_status_t*);
void systask_do_gettime       (Systask_msg_t*, Msg_status_t*);
void systask_do_umalloc       (Systask_msg_t*, Msg_status_t*);
void systask_do_pci_set_config(Systask_msg_t*, Msg_status_t*);
void systask_do_poll          (Systask_msg_t*, Msg_status_t*);
void systask_do_get_core      (Systask_msg_t*, Msg_status_t*);
void systask_do_invalid       (Systask_msg_t*, Msg_status_t*);
void systask_do_ps            (Systask_msg_t*, Msg_status_t*);
void systask_do_getstdio      (Systask_msg_t*, Msg_status_t*);
void systask_do_redirect      (Systask_msg_t*, Msg_status_t*);
void systask_do_sigaction     (Systask_msg_t*, Msg_status_t*);
void systask_do_sigreturn     (Systask_msg_t*, Msg_status_t*);
void systask_do_alarm         (Systask_msg_t*, Msg_status_t*);
void systask_do_eoi           (Systask_msg_t*, Msg_status_t*);
void systask_do_map_dma       (Systask_msg_t*, Msg_status_t*);
void systask_do_msi           (Systask_msg_t*, Msg_status_t*);
#ifdef KERNEL_DEBUG
void systask_do_kprintint     (Systask_msg_t *, Msg_status_t *);
void systask_do_kprintstr     (Systask_msg_t *, Msg_status_t *);
#endif 

void systask_do_map_mmio      (Systask_msg_t*, Msg_status_t*);
void systaks_do_map_dma       (Systask_msg_t*, Msg_status_t*);

#ifdef ENABLE_SMP
void systask_do_driver_init_done (Systask_msg_t*, Msg_status_t*);
#endif
