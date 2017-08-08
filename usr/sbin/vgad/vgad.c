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
 * vgad.c  -  VGA device driver.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>			/* For int types        */
#include <syscall.h>
#include <time.h>			/* For clockid_r	*/
#include <msg.h>			/* Message Passing API	*/

#include <sbin/syspid.h>
#include <sbin/kbd.h>			/* For KBD's PID	*/
#include <sbin/vgad.h>			/* For VGA functions    */

static void main_loop();
static void vgad_init();

/* Wrapper Functions */
static void vgad_putc   (Vgad_msg_t*, Msg_status_t*);
static void vgad_retreat(Vgad_msg_t*, Msg_status_t*);
static void vgad_puts   (Vgad_msg_t*, Msg_status_t*);

/* Helper functions */
static void vgad_draw_char(char c);

/* These numbers must match the syscall types in kmsg.h */
#define VGA_MSGTYPE_MAX  2
#define VGA_NUM_MSGTYPES (VGA_MSGTYPE_MAX + 1)

/* For handling bad messages */
static void vgad_invalid(Vgad_msg_t*, Msg_status_t*);
static char err_str[] = "VGAD_ERR";
#define ERR_STR_LEN 8

static void (*func_array[VGA_NUM_MSGTYPES])(Vgad_msg_t*, Msg_status_t*) = 
{ vgad_putc, vgad_retreat, vgad_puts };


#ifdef SERIAL_OUT_USER
#define PORT 0x3f8   /* COM1 */
int is_transmit_empty() {
  return inb(PORT + 5) & 0x20;
}

void write_serial(char a) {
  while (is_transmit_empty() == 0);
  outb(PORT,a);
}
#endif /* SERIAL_OUT_USER */

/*
 * Function: main()
 * Description: Entry point of the VGA driver.
 */
int main() {
  int mypid;
  mypid = getpid();
  if(mypid!=VGAD) {
    printf("[vgad: initialized with pid %d, expected %d\n",mypid,VGAD);
    exit(EXIT_FAILURE);
  }
  
  vgad_init();
  main_loop();
}

/*
 * Function: main_loop()
 *
 * Description: Main driver loop for servicing requests.
 */
static void main_loop() {
  Vgad_msg_t msg;
  Msg_status_t status;
  int *ptype;
  int ret;
  
  ptype = (int*)(&msg);

  while(1) {
    ret = msgrecv(ANY, &msg, sizeof(Vgad_msg_t), &status);
    if (ret >= 0) 
      {
	if ((*ptype) >= 0 && (*ptype) < VGA_NUM_MSGTYPES) {
	  (*func_array[*ptype])(&msg,&status);
	} else {
	  vgad_invalid(&msg,&status);
	}
      }
  }
}

/*
 * Function: vgad_init()
 * Description: Sets up VGA memory in the process's address space.
 */
void vgad_init() {
  uint16_t *screen;
  
  /* Map VGA memory into address space */
  screen = map_vga_mem();
  if (screen == NULL)
    exit(1);
  /* Point vga functions at correct memory addr */
  vga_set_loc(screen);
  //	vga_setpos(0,0);
}

/*
 * VGA WRAPPER FUNCTIONS 
 *
 * Description: These functions are simply wrappers for the vga
 * functions. The VGA functions cannot be modified because they must
 * be used by the kernel as well as the hypervisor, etc. yet I want to
 * use a table of function pointers... hence I must use wrapper funcs.
 */
void vgad_putc(Vgad_msg_t *msg, Msg_status_t *status) {
  Drawc_req_t *req;
  Drawc_resp_t resp;
  
  req = (Drawc_req_t *)msg;
  
  vgad_draw_char(req->c);
  
  resp.type = VGA_DRAWC;
  resp.ret = 0;
  msgsend(status->src, &resp, sizeof(Drawc_resp_t));
}

void vgad_retreat(Vgad_msg_t *msg, Msg_status_t *status) {
  Retreat_req_t *req;
  Retreat_resp_t resp;
  
  req = (Retreat_req_t *)msg;
  
  if (status->src == KBD)
    vga_retreat(req->cols);
  
  resp.type = VGA_RETREAT;
  resp.ret = 0;
  msgsend(status->src, &resp, sizeof(Retreat_resp_t));
}

static void vgad_puts(Vgad_msg_t *msg, Msg_status_t *status) {
  Puts_req_t *req;
  Puts_resp_t resp;
  uint32_t i, len;

  req = (Puts_req_t *)msg;
  len = req->len;
  for(i=0; i<len; i++)
    vgad_draw_char((req->buf)[i]);
  resp.type = VGA_PUTS;
  resp.ret = 0;
  msgsend(status->src, &resp, sizeof(Puts_resp_t));
}

void vgad_invalid(Vgad_msg_t *msg, Msg_status_t *status) {
  int i;
  for(i=0; i<ERR_STR_LEN; i++)
    vgad_draw_char(err_str[i]);
}

/* Private Helper Functions */

static void vgad_draw_char(char c) {
  
  if((c) == '\n'){
#ifdef SERIAL_OUT_USER
    write_serial(0xA);
#endif	/* SERIAL_OUT_USER */
    vga_newline();
  }
  else if((c) == '\t') {
#ifdef SERIAL_OUT_USER
    write_serial(0x9);
#endif	/* SERIAL_OUT_USER */
    vga_tabline();
  }
  else {
#ifdef SERIAL_OUT_USER
    write_serial(c);
#endif	/* SERIAL_OUT_USER */
    vga_drawc(c, STD_VGA_COLOR);
    vga_advance(1);
  }
}

