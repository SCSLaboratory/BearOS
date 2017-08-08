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
 * keyboard.c 
 * 
 * Description: The userland keyboard driver for Bear.  The strategy
 * here is to decrease code size and complexity by letting the message
 * passing system queue requests, instead of doing it explicitly.
 */
#include <stdlib.h>
#include <string.h>	  /* For memcpy */
#include <stdio.h>
#include <msg.h>	  /* For msgsend/msgrecv */
#include <msg_types.h>
#include <signal.h>

#include <sbin/syspid.h>
#include <sbin/pio.h>	  /* For Port IO */
#include <sbin/kbd.h>
#include <sbin/vgad.h>
#include <sbin/kb_scancodes.h> /* For scancodes */

/* Keyboard communication */
#define KB_DATA 0x60	  /* For receiving keyboard scancodes */
#define KB_CTRL 0x64	  /* For acknowledging keys */
#define KBIT	0x80	  /* bit used to ack characters to keyboard */

/* Keyboard Status */
static volatile uint8_t      shifted;   /* Is shift held down? */
static volatile uint8_t      escaped;   /* Are we reading an escaped sequence? */
static volatile uint8_t      capslock;  /* Caps lock on? */
static volatile uint8_t      ctrled;  	/* left ctrl key pressed? */

/* Keyboard Buffer */
static char kb_buf[MAX_STR_SZ];/* Key buffer */
static unsigned int buf_pos;   /* How many chars waiting in buf */

/* Current Request */
static int req_pid;       /* Who asked for a string? */

/* Message to the VGAD asking to draw the character */
static int kbputchar(char c);
static int vga_backup(int cols);

/* This tells us the size of largest message we can expect. */
typedef union {
  Getstr_req_t getstr_req;
  Hwint_msg_t keyb_int;
} Keyb_msg_t;

/*
 * *** DRIVER FUNCTIONS ***
 *
 * There are two driver functions: handle_keypress() and
 * handle_kb_request().  Handle_keypress() is called whenever an
 * interrupt on IRQ1 comes in.  Handle_kb_request() is called when a
 * new getchar/getstr request is fielded.
 *
 * Note that ONLY ONE request is handled at a time.  After a request
 * comes in, handle_kb_request() is called; this function does not
 * return until a full string has been read in from the keyboard
 * (ending when the user pushes the return key).  THEN the function
 * returns, and a new request may be serviced.
 *
 * The idea is to reduce the codebase by relying on the message
 * passing interface to queue requests, rather than doing so
 * explicitly here.
 */
 
/* These are the functions called when a message comes in. */
static void handle_keypress(Keyb_msg_t*, Msg_status_t*);
static void handle_kb_request(Keyb_msg_t*, Msg_status_t*);

/* These numbers must match the syscall types in kmsg.h */
#define KEYB_MSGTYPE_MAX  1
#define KEYB_NUM_MSGTYPES (KEYB_MSGTYPE_MAX + 1)

static void (*func_array[KEYB_NUM_MSGTYPES])(Keyb_msg_t*, Msg_status_t*) = 
 { handle_keypress, handle_kb_request };

/* Other Functions */
static void handle_invalid(Keyb_msg_t*, Msg_status_t*);
static void kb_init();
static void clear_buf();
static int scan_keyboard();
static int translate_scancode(uint8_t,char*);
static void complete_req();
static void main_loop();

/*
 * Function: main()
 * Description: The start point...
 */
int main(void) 
{
  kb_init();
  main_loop();
  return -1;
}

/*
 * Function: main_loop()
 * Description: Main driver loop for servicing requests.
 */
static void main_loop() {
  Keyb_msg_t msg;
  Msg_status_t status;
  int *ptype;
  int ret;

  ptype = (int*)(&msg);
  
  while(1) {

    ret = msgrecv(ANY, &msg, sizeof(Keyb_msg_t), &status);

    if (ret >= 0) {
      if ((*ptype) >= 0 && (*ptype) <= KEYB_MSGTYPE_MAX) {
	(*func_array[*ptype])(&msg,&status);
      } else {
	handle_invalid(&msg, &status);
      }
    }
  }
}


/*
 * Function: kb_init()
 * Description: Initializes the driver.
 */
static void kb_init() {
  /* Init static vars */
  shifted = 0;
  escaped = 0;
  capslock = 0;

  /* Init char buffer */	
  clear_buf();
  //assume a key has been pressed or we'll never see interrupts again 
  // if a surpious interrupt occured the kdb automatically ack's it
  //so we need to strobe it or we'll never boot up
  scan_keyboard();  
		
/*
/* hack to stop spurious '$' character from initial 
/* kernel to userland switch (RMD)
*/
#if 0
//#ifdef ENABLE_SMP 
  buf_pos--;
#endif
  req_pid = -1;
}


/*
 * Function: clear_buf()
 * Description: Resets the character buffer.
 */
static void clear_buf() {
  memset(kb_buf,'\0',MAX_STR_SZ);
  buf_pos = 0;	
}


/*
 * Function: handle_kb_request()
 * Description: Add a character/string request onto the list.
 */
static void handle_kb_request(Keyb_msg_t *msg, Msg_status_t *status) {
  Getstr_resp_t resp;
  int pid;

	pid = status->src;

	/* Debug: Already handling a request? */
	if (req_pid != -1) {
		printf("[kbd] Got request when already handling a request?\n");
		resp.type = KEYB_GETSTR;
		resp.slen  = -1;
		msgsend(pid, &resp, sizeof(Getstr_resp_t));
		return;
	}

	/* Save request */
	req_pid = pid;

	/* Wait for string from hardware */
	while(req_pid != -1) {
		msgrecv(HARDWARE, msg, sizeof(Keyb_msg_t), status);
		handle_keypress(msg, status);
	}
}


/*
 * Function: handle_keypress()
 * Description: Interacts with the keyboard through port I/O.  Waits til KB is
 *              ready (may not be true, despite interrupt), receives the
 *              scancode, translates it into a keypress, and takes the
 *              appropriate action (either updating kb_buf or updating the
 *              variables for shift, caps lock, etc.)
 */
static void handle_keypress(Keyb_msg_t *msg, Msg_status_t *status) {
  char new_char;
  int print;
  uint8_t code;

  /* Get the scancode from the keyboard */
  code = scan_keyboard();
  /* Parse the Scancode */
  print = translate_scancode(code, &new_char);
  /*
   * hack to stop spurious '$' character from initial 
   * kernel to userland switch (RMD)
  */
#if 0
  //#ifdef ENABLE_SMP
  if(buf_pos == -1){
    clear_buf();
    return;
  }
#endif
  /* Print character and service request if user pushed enter.  */
  if (print) {
    kbputchar(new_char);

    kb_buf[buf_pos++] = new_char;
    if (buf_pos >= MAX_STR_SZ)
      buf_pos = MAX_STR_SZ - 1;
    
    if (new_char == '\n') 
      complete_req(); 
  }
}

static void handle_invalid(Keyb_msg_t* msg, Msg_status_t* status) {
  asm volatile("hlt");
  printf("Error: Keyb received bad message!\n"); 
}

/*
 * Function: scan_keyboard()
 * Description: Get the character from the KB hardware and acknowledge it.
 */
static int scan_keyboard() {
  int code;
  int val;
  
  /* Send a message to the kernel asking for the character from the KB hardware */
	
  code = inb(KB_DATA);       /* get the scan code for the key struck */
  val = inb(KB_CTRL);         /* strobe the keyboard to ack the char  */
  outb(KB_CTRL, val | KBIT);  /* strobe the bit high                  */
  outb(KB_CTRL, val);	       /* now strobe it low                    */
  return code;
}

/*
 * Function: translate_scancode()
 * Description: Translates the keyboard scan code into a character or "special
 *              key" keypress/keyrelease.
 */
static int translate_scancode(uint8_t code, char *new_char) {
  /* Were we escaped? */
  if (escaped) {
    escaped = 0;
  }

  switch(code) {
  case 0x00: printf("KEYBOARD ERROR\n"); break;
  case 0x2a: shifted 	= 	1;  break; 	/* left shift pressed */
  case 0xaa: shifted 	= 	0;  break; 	/* left shift released */
  case 0x36: shifted 	= 	1;  break; 	/* right shift pressed */
  case 0xB6: shifted 	= 	0;  break; 	/* right shift released */
  case 0xe0: escaped 	= 	1;  break; 	/* receiving an escaped scancode seq. */
  case 0x3a: capslock ^= 	1; 	break; 	/* caps lock pressed */
  case 0x1d: ctrled  	= 	1;	break; 	/* left ctrl pressed */
  case 0x9d: ctrled		=		0;	break;	/* left ctrl released */
  case 0x0e: /* Backspace */
    if (buf_pos != 0) {
      buf_pos--;
      kb_buf[buf_pos] = '\0';  /* Update buf */
      vga_backup(1);          /* Update screen */
      kbputchar('\0');
      vga_backup(1);
    }
    break;
    
  default:                             /* a keypress or release */
    if (!(code & 0x80) && !(ctrled) ) {                /* Not a key release? */
      *new_char = ((shifted || capslock) ? uppercase:lowercase)[code];
      return 1;
    }
    else if (!(code & 0x80) && (ctrled)) {
      /* We will want to return a unique identifier to send a message
       * to these handlers. One possibilitiy is setting new_char to
       * return 0xf0, 0xf1, etc. As they are unusued looking at the
       * osdev list. 0xE0 is the extended scan code identifier. We do
       * not support extended scan codes currently. Thus, right
       * control may not work in this setup. If we wanted to support
       * extended scan codes we would first need to grab 0xE0 and then
       * grab the next code from the keyboard. This would be handled
       * in handle keypress. NOTE on some keyboards the right control
       * returns the same scancode as left control and it works. (RMD)
       */
      switch (code){
	/*
	 * poorly impimented kill -- uses user_pid_counter to kill the
	 * last spawned process. Need a better way to lookup what is
	 * running. Problem arises from keyboard daemon being a user
	 * level process. So, when waitpid is called we can't call
	 * ksched_get_last to get the actual user process, as this
	 * returns the pid for the keyboard daemon. I'm unsure as what
	 * other solution we may leverage or have to implement to fix
	 * this. (RMD)
	 */
      case 0x2e: kill(1/*getpid()*/, SIGINT);        break;
      case 0x2c: kill(1/*getpid()*/, SIGTSTP);       break;
      case 0x2b: kill(1/*getpid()*/, SIGQUIT);       break;
      case 0x14: printf("ctrl^T  pressed\n");   break;  /* future signal support */
      default:	break;
      }
    }
  }
  return 0;
}


/*
 * Function: complete_req()
 *
 * Description: Respond to the pending request, if it exists.
 */
static void complete_req() {
  Getstr_resp_t resp;
  unsigned int cp_len;
  
  if (req_pid != -1) {
    cp_len = buf_pos + 1;
    resp.type = KEYB_GETSTR;
    resp.slen = cp_len;
    memcpy(resp.str, kb_buf, cp_len);
    msgsend(req_pid, &resp, sizeof(Getstr_resp_t));
  }
  /* Reset request state */
  clear_buf();
  req_pid = -1;
}

/* Puts a character on the screen. */
static int kbputchar(char c) {
  Drawc_req_t req;
  Drawc_resp_t resp;
  Msg_status_t status;
  
  req.type = VGA_DRAWC;
  req.c    = c;
  msgsend(VGAD,&req,sizeof(Drawc_req_t));
  msgrecv(VGAD,&resp,sizeof(Drawc_resp_t),&status);
  return resp.ret;
}

static int vga_backup(int cols) {
  Vgaback_req_t req;
  Vgaback_resp_t resp;
  Msg_status_t status;
  req.type = VGA_RETREAT;
  req.cols = cols;
  msgsend(VGAD,&req,sizeof(Vgaback_req_t));
  msgrecv(VGAD,&resp,sizeof(Vgaback_resp_t),&status);
  return resp.ret;
}

