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
#include <constants.h>
#include <stdint.h>
#include <proc.h>

/* IDT constants */
#define NUM_EXCEPTIONS 0x20  /* Number of exceptions specified by Intel x86 */
#define INTR_OFFSET 0x20     /* IRQs are offset to avoid execption conflicts */
#define INTR64_OFF 0b01101110/* Disabled 32/64-bit Interrupt gate in hex 0x6E*/
#define INTR64_ON  0b11101110/* Enabled 32/64-bit Interrupt gate in hex 0xEE */
#define TRAP64_ON  0x8F
#define IDT_NUM_ENTRIES 256  /* Number of IDT table entries */

/* TSS Constants */
#define TSS_ENTRY_OFFSET 0x28 /* Offset to the TSS entry in the GDT */
#define TSS_SIZE 104          /* In bytes - total size, not waht goes in GDT */

/* PIT Constants */
/*
Calcualting the pit time: solve for x to get the desired frequency
x = 1193182 / f  and remember f = 1/T   

so if we want a 10ms timer  1/0.01 = 100Hz;  1193182/100 = 1193182
convert that to hex 0x2e9b   And load into tthe high and low bytes below */
/* the 1193182 constant comes from the old days of NTSC TV's, google it */

/* 100US : 1 /.0001 = 10000 ; 1193182/10000 = 119.31  in hex 0x77 */

#define R100US_LO   0x76 //was 77 but rounding errors so -1
#define R100US_HI   0x00

#define R2MS_LO     0x52
#define R2MS_HI			0x09

#define R3MS_LO     0xFC       /* Triggers an interrupt every ~3ms  */
#define R10MS_LO	0x9B
#define R3MS_HI     0x0D       /* Value 0x0DFC split into two bytes */
#define R10MS_HI	0x2E

/*if you use micro seconds this is modified by *100 over in ktimer.c */
#define MS_PER_TICK	1

#define CHANNEL0    0x40       /* Data register for PIT Channel 0   */
#define PIT_CTRL    0x43       /* Control/Mode register for PIT     */
#define PIT_MODE 0b00110110    /* Channel 0, lo-hi, square wave     */

/* PIC constants */
#define PIC1          0x20     /* IO base address of Master PIC */
#define PIC2          0xA0     /* IO base address of slave pic */
#define PIC1_COMMAND  PIC1
#define PIC1_DATA     (PIC1+1)
#define PIC2_COMMAND  PIC2
#define PIC2_DATA     (PIC2+1)

#define ICW1_ICW4	   0x01    /* ICW4 (not) needed */
#define ICW1_SINGLE	   0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL     0x08    /* Level triggered (edge) mode */
#define ICW1_INIT      0x10    /* Initialization - required! */

#define PIC_OFFSET1    0x20    /* IMPORTANT: This is where IRQs begin */
#define PIC_OFFSET2    0x28
#define PIC1_MASK      0b00000000 /* Let em all through */
#define PIC2_MASK      0b00000000
 
#define ICW4_8086       0x01   /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02   /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08   /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C   /* Buffered mode/master */
#define ICW4_SFNM       0x10   /* Special fully nested (not) */

typedef struct _idtentry {
	uint16_t base_low;  /* offset bits 0-15 */
	uint16_t selector;  /* code segment selector in GDT or LDT */
	uint8_t ist;        /* IST index; top 4 bits zeroed */
	uint8_t type;       /* True interrupt or trap? */
	uint16_t base_mid;  /* offset bits 16-31 */
	uint32_t base_high; /* offset bits 32-63 */
	uint32_t reserved;  /* unused, must be zero */
} __attribute__ ((packed)) idtentry_t;


struct idtptr {
	uint16_t limit;
	uint64_t base;
} __attribute__ ((packed));

typedef struct tss {
	uint32_t rsp_low;
	uint32_t rsp_high;
} __attribute__ ((packed)) tss_t;

typedef struct _gdtentry {
	uint8_t c0;
	uint8_t c1;
	uint8_t c2;
	uint8_t c3;
	uint8_t c4;
	uint8_t c5;
	uint8_t c6;
	uint8_t c7;
} __attribute__ ((packed)) Gdtentry_t;

typedef struct _tssdesc_t {
	uint8_t c0;
	uint8_t c1;
	uint8_t c2;
	uint8_t c3;
	uint8_t c4;
	uint8_t c5;
	uint8_t c6;
	uint8_t c7;
	uint32_t upper_addr;
	uint32_t zeros;
} Tssdesc_t;

/* GDT/LDT and IDT. */

struct gdt_desc {
	uint16_t limit;
	uint64_t base;
} __attribute__ ((packed));

struct idt_desc {
	uint16_t limit;
	uint64_t base;
} __attribute__ ((packed));


/* Wait for IO to happen.  Best way to do this is perform an IO that doesn't
*  do anything.  For use only when there is no status register/IRQ to tell us
*  things have worked.
*  Address 0x80 is apparently used for "checkpoints" during POST.  The linux
*  kernel seems to think it's free for use...
*/
#define io_wait asm volatile("outb %%al, $0x80" : : "a"(0))

void intr_init();
void hypv_intr_init();
void init_new_gdt();
void intr_start_timer();
void intr_invoke_handler(uint64_t vector);
void intr_add_handler(uint8_t i, void (*func)(unsigned int, void*),void*arg);
void intr_update_idtentry(uint8_t i, int type, uint64_t func);

void intr_set_mask(unsigned char irq);
void intr_clear_mask(unsigned char irq);


void ap_lidt();
static volatile struct idtptr idtp;


/* Used by bootloader */
void idt_set_offset(idtentry_t *interrupt, uint64_t addr);
void init_pic();

/* Used by exception handlers */
void print_exception_info_one(uint64_t);
void print_exception_info_two(uint64_t,uint64_t*,uint64_t,uint64_t,uint64_t*,uint64_t);

/* Assembly routine to restore user process */
void restore_user_proc(Proc_t *);

/** Assembly routine to save kernel context */
void save_kernel_context();

/** Assembly routine to restore kernel context */
void restore_kernel_proc(Proc_t *);

Proc_t  *interrupt_get_last(void);

void interrupt_acquire_lock(void);

void interrupt_release_lock(void);

