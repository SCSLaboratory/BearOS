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
/*
 * syscall.c
 *
 * System calls that are *NOT* implemented by newlib 
*/

#include <stdint.h>
#include <time.h>
#include <syscall.h>
#include <string.h>
#include <msg.h>
#include <stdio.h>
#include <swint.h>

#include <unistd.h>
#include <signal.h>

#include <sbin/vgad.h>
#include <sbin/kbd.h>
#include <sbin/sysd.h>


/******************************************************************************
 **** USER-LEVEL SYSCALLS *****************************************************
 *****************************************************************************/
/* These syscalls may be invoked by any process. */

/* allow user to force vm to exit */
int force_vmexit(uint64_t int1, uint64_t int2, void *strct_1){
  swint_2();	
  return 0;
}
#ifdef KERNEL_DEBUG
/* pass an integer to the kernel for printing */
void kprintint(char *strp,int val,int src) {
  kprintint_req_t req;
  kprintint_resp_t resp;
  Msg_status_t status;

  req.type = SC_PRINTINT;
  req.val = val;
  strncpy(req.str,strp,MAXPRSTR);
  req.str[MAXPRSTR-1]='\0';
  req.src = src;
  msgsend(SYS, &req, sizeof(kprintint_req_t));
  msgrecv(SYS, &resp, sizeof(kprintint_resp_t), &status);
}

/* pass a string to the kernel for printing */
void kprintstr(char *strp) {
  kprintstr_req_t req;
  kprintstr_resp_t resp;
  Msg_status_t status;

  req.type   = SC_PRINTSTR;
  strncpy(req.str,strp,MAXPRSTR);
  req.str[MAXPRSTR-1]='\0';
  msgsend(SYS, &req, sizeof(kprintstr_req_t));
  msgrecv(SYS, &resp, sizeof(kprintstr_resp_t), &status);
}
#endif

/* Gets a string from the stdin */
int getstr(char *s, int len) {
  Getstr_req_t req;
  Getstr_resp_t resp;
  Msg_status_t status;
  int pid;

  pid = getstdio(0);
  req.type = KEYB_GETSTR;
  /* communicate with process providing stdin */
  msgsend(pid,&req,sizeof(Getstr_req_t));
  msgrecv(pid,&resp,sizeof(Getstr_resp_t),&status);
  /* copy the received string into the target buffer */
  strncpy(s,resp.str,(len < (resp.slen)) ? len : (resp.slen));
  return (len < resp.slen) ? len : resp.slen;
}

/* Sleeps for the given number of milliseconds. */
void bsleep(int ms) {
  Usleep_req_t req;
  Usleep_resp_t resp;
  Msg_status_t status;
  
  if (ms <= 0) return;
  
  req.type   = SC_USLEEP;
  req.slp_ms = ms;
  msgsend(SYS, &req, sizeof(Usleep_req_t));
  msgrecv(SYS, &resp, sizeof(Usleep_resp_t), &status);
}

unsigned sleep(unsigned int sec) {
  bsleep(sec*1000);
  return 0;
}

/* queries various clocks.  Right now we just have CLOCK_MONOTONIC. */
int clock_gettime(clockid_t clk_id, struct timespec *tp) {
  Gettime_req_t req;
  Gettime_resp_t resp;
  Msg_status_t status;
  
  if (tp == NULL)
    return -1;
  
  req.type = SC_GETTIME;
  req.clk_id = clk_id;
  msgsend(SYS, &req, sizeof(Gettime_req_t));
  msgrecv(SYS, &resp, sizeof(Gettime_resp_t), &status);
  
  if (resp.ret == 0)
    memcpy(tp, &(resp.tspec), sizeof(struct timespec));
  
  return resp.ret;
}

int getstdio(int fd) {
  getstdio_req_t req;
  getstdio_resp_t resp;
  Msg_status_t status;

  req.type = resp.type = SC_GETSTDIO;
  req.fd = fd;

  req.tag = resp.tag = get_msg_tag();

  msgsend(SYS, &req, sizeof(getstdio_req_t));
  msgrecv(SYS, &resp, sizeof(getstdio_resp_t), &status);
  if ( resp.pid == 0 )
    asm volatile("hlt");
  return resp.pid;
}

void redirect(int inpid,int outpid,int errpid) {
  redirect_req_t req;
  redirect_resp_t resp;
  Msg_status_t status;

  req.type = SC_REDIRECT;
  req.inpid = inpid;
  req.outpid = outpid;
  req.errpid = errpid;
  msgsend(SYS, &req, sizeof(redirect_req_t));
  msgrecv(SYS, &resp, sizeof(redirect_resp_t), &status);
}


/*******************************************************************************
 **** SERVER-LEVEL SYSCALLS ****************************************************
 ******************************************************************************/

/* These syscalls may only be invoked by server processes and device drivers. */

uint16_t* map_vga_mem() {
  Vgamem_req_t req;
  Vgamem_resp_t resp;
  Msg_status_t status;
  req.type = SC_VGAMEM;
  msgsend(SYS,&req,sizeof(Vgamem_req_t));
  msgrecv(SYS,&resp,sizeof(Vgamem_resp_t),&status);
  return resp.vaddr;
}

void reboot() {
  Reboot_req_t req;
  Reboot_resp_t resp;
  Msg_status_t status;
  req.type = SC_REBOOT;
  msgsend(SYS,&req,sizeof(Reboot_req_t));
  msgrecv(SYS,&resp,sizeof(Reboot_resp_t),&status);
  /* Not reached */
  printf("reboot(): Reached something unreachable?!\n");
}

void unmask_irq(unsigned char irq) {
  Unmask_msg_t msg;
  msg.type = SC_UNMASK;
  msg.irq = irq;
  msgsend(SYS, &msg, sizeof(Unmask_msg_t));
}


int enable_msi(unsigned char irq, int level, int dest, int mode, int trigger, int bus, int dev, int func  ) {
  Msi_en_req_t msg;
  Msi_en_resp_t resp;
  Msg_status_t status;
    
  msg.type = SC_MSI_EN;
  msg.vector = irq;
  msg.trigger_level  = level; 
  msg.apic_dst = dest;
  msg.delv_mode    = mode;
  msg.trigger_mode = trigger;
  msg.bus     = bus;
  msg.dev      = dev; 
  msg.func       = func;
  msgsend(SYS, &msg, sizeof(Msi_en_req_t));
  msgrecv(SYS, &resp, sizeof(Msi_en_resp_t), &status);
  
  return resp.ret;
  
}



void user_eoi() {
	Eoi_msg_t msg;
	msg.type = SC_EOI;
	msgsend(SYS, &msg, sizeof(Eoi_msg_t));
}
