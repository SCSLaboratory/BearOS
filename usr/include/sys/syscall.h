#pragma once
/*
 * syscall.h -- form of system call interface
 *
 */
#include <stdint.h>
#include <msg.h>
#include <sbin/syspid.h>
#include <sbin/pci_structs.h>
#include <signal.h>

#ifdef USER
#include <time.h>
#include <sys/types.h>
#else
#include <ktime.h>
#endif

#define MAX_FNAME_SZ 64
#define MAX_PS_SZ 64

/* SYSCALL MESSAGE TYPES */
#define HARD_INT    0   /* All hardware interrupts */
#define SC_FORK     1   /* fork()                  */
#define SC_EXEC     2   /* exec()                  */
#define SC_GETPID   3   /* getpid()                */
#define SC_WAITPID  4   /* waitpid()               */
#define SC_EXIT     5   /* exit()                  */
#define SC_KILL     6   /* kill()                  */
#define SC_SPAWN    7   /* spawn()                 */
#define SC_USLEEP   8  /* usleep()                */
#define SC_VGAMEM   9  /* set_vga_mem()           */
#define SC_REBOOT   10  /* reboot()                */
#define SC_UNMASK   11  /* unmask_intr()           */
#define SC_GETTIME  12  /* clock_gettime()         */
#define SC_UMALLOC  13  /* user call to malloc     */
#define SC_PCI_SET  14  /* Set PCI configuration   */
#define SC_POLL	    15	/* Poll                    */
#define SC_CORE	    16	/* Get Core                */
#define SC_PS       17  /* ps                      */
#define SC_GETSTDIO 18
#define SC_REDIRECT 19

#ifdef KERNEL_DEBUG
#define SC_PRINTINT 20	/* print an int for kernel debugging */
#define SC_PRINTSTR 21	/* print an str for kernel debugging */
#define SC_PRINT_ADDR 28
#endif

#define SC_SIGACT   22  /* sigaction()             */
#define SC_SIGRET   23  /* signal return mechanism */
#define SC_ALARM    24	/* alarm() -- schedule a signal */
#define SC_EOI      25
#define SC_MAP_MMIO 26
#define SC_MAP_DMA  27
#define SC_MSI_EN  29
/*note next number is 30 !!!! */

/* fork */
typedef struct {
  int type;
} Fork_req_t;

typedef struct {
  int type;
  int ret;
} Fork_resp_t;

/* exec */
typedef struct {
  int type;
  char fname[MAX_FNAME_SZ];
  int argc;
  int num_env;
  char **argv;
  char **env;
  char *binary_location;
  int binary_size;
} Exec_req_t;

typedef struct {
  int type;
  int ret;
} Exec_resp_t;

/* getpid */
typedef struct {
  int type;
  unsigned int tag;
} Getpid_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int pid;
} Getpid_resp_t;

/* waitpid */
typedef struct {
  int type;
  unsigned int tag;
  int wpid;
  int opts;
} Waitpid_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int wpid;
  int exit_val;
} Waitpid_resp_t;

/* exit */
typedef struct {
  int type;
  int exit_sig; 
} Exit_req_t;

typedef struct {
  int type;
} Exit_resp_t;

/* kill */
typedef struct {
  int type;
  unsigned int tag;
  int kill_sig;
  int kill_pid; 
} Kill_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int ret_val;
} Kill_resp_t;

/* spawn */
typedef struct {
  int type;
  char fname[MAX_FNAME_SZ];
} Spawn_req_t;

typedef struct {
  int type;
  int ret;
} Spawn_resp_t;

/* usleep */
typedef struct {
  int type;
  int slp_ms;
} Usleep_req_t;

typedef struct {
  int type;
} Usleep_resp_t;

/* vgamem */
typedef struct {
  int type;
} Vgamem_req_t;

typedef struct {
  int type;
  uint16_t *vaddr;
} Vgamem_resp_t;

/* reboot */
typedef struct {
  int type;
} Reboot_req_t;

typedef struct {
  int type;
} Reboot_resp_t;

/* unmask */
typedef struct {
  int type;
  unsigned char irq;
} Unmask_msg_t;

/* unmask */
typedef struct {
  int type;
} Eoi_msg_t;

/* clock_gettime */
typedef struct {
  int type;
  clockid_t clk_id;
} Gettime_req_t;

typedef struct {
  int type;
  int ret;
  struct timespec tspec;
  struct tm tm_local;
} Gettime_resp_t;

/* umalloc */
typedef struct {
  int type;
  int bytes;
} Umalloc_req_t;

typedef struct {
  int type;
  int ret_val;
  uint64_t addr;
} Umalloc_resp_t;

/* pci_set_config */
typedef struct {
  int type;
  Pci_dev_t pci_dev;
  unsigned int offset;
  uint32_t val;
  unsigned int width;
} Pci_set_config_req_t;

typedef struct {
  int type;
  int ret_val;
} Pci_set_config_resp_t;

typedef struct{
  int type;
  unsigned int tag;
  Pci_dev_t pci_dev;
  int bar;
}Map_mmio_req_t;

typedef struct{
  int type;
  unsigned int tag;
  uint64_t *virt_addr; 
}Map_mmio_resp_t;


typedef struct {
  int type;
  unsigned int tag;
  uint64_t num_bytes;
}Map_dma_req_t;

typedef struct {
  int type;
  unsigned int tag;
  uint64_t *phys_addr;
  uint64_t *virt_addr; 
}Map_dma_resp_t;


/* poll */
typedef struct {
  int type;
  char poll_structure[1024]; /* This should be a a pointer to a struct */	
  int nfds; /* Number of FDs */
  int timeout;
} Poll_req_t;

typedef struct {
  int ret_val;
} Poll_resp_t;

/* get_core */
typedef struct {
  int type;
} Get_core_req_t;

typedef struct {
  int ret_val;
} Get_core_resp_t;

/* ps */

typedef struct {
  int type;
} Ps_req_t;

typedef struct {		/* will eventually provide the -- currently printed in kernel */
  int type;
  int ret;
  int entries;			/* entries in the ps table */
  char status[MAX_PS_SZ];
  int pid[MAX_PS_SZ];
  char procnm[MAX_PS_SZ][MAX_FNAME_SZ];
} Ps_resp_t;

/* getstdio */
typedef struct {
  int type;
  unsigned int tag;
  int fd;
} getstdio_req_t;

typedef struct {
  int type;
  unsigned int tag;
 int pid;
} getstdio_resp_t;

/* redirect */
typedef struct {
  int type;
  int inpid;
  int outpid;
  int errpid;
} redirect_req_t;

typedef struct {
  int type;
} redirect_resp_t;

#ifdef KERNEL_DEBUG

#define MAXPRSTR 512
/* kprintint */
typedef struct {
  int type;
  int val;
  char str[MAXPRSTR];
  int src;
} kprintint_req_t;

typedef struct {
  int type;
} kprintint_resp_t;

/* kprintstr*/
typedef struct {
  int type;
  char str[MAXPRSTR];
} kprintstr_req_t;

typedef struct {
  int type;
} kprintstr_resp_t;

typedef struct {
 int type;
 unsigned int tag;
 uint64_t addr; 
 int bytes;	
} kprintaddr_req_t;
	
#endif

/* signal */
typedef struct {
  int type;
  unsigned int tag;
  int signo;
  struct sigaction act;
  int (*sigret)(void);
} Sigact_req_t;

typedef struct {
  int type;
  unsigned int tag;
  int ret_val;
  struct sigaction oldact;
} Sigact_resp_t;

typedef struct {
  int type;
} Sigret_req_t;

typedef struct {
	int type;
	int seconds;
} Alarm_req_t;

typedef struct {
	int type;
	int ret;
} Alarm_resp_t;

typedef struct {
  int type;
  unsigned char irq;
  int trigger_level;
  int vector;
  int apic_dst;
  int delv_mode;
  int trigger_mode; 
  int bus; 
  int dev; 
  int func; 
} Msi_en_req_t;

typedef struct {
	int type;
	int ret;
} Msi_en_resp_t;



/* This provides the maximum msg size the systask expects to recieve */
typedef union {
  Fork_req_t fork_req;
  Fork_resp_t fork_resp;
  Exec_req_t exec_req;
  Exec_resp_t exec_resp;
  Getpid_req_t getpid_req;
  Getpid_resp_t getpid_resp;
  Waitpid_req_t waitpid_req;
  Waitpid_resp_t waitpid_resp;
  Exit_req_t exit_req;
  Exit_resp_t exit_resp;
  Kill_req_t kill_req;
  Kill_resp_t kill_resp;
  Spawn_req_t spawn_req;
  Spawn_resp_t spawn_resp;
  Usleep_req_t usleep_req;
  Usleep_resp_t usleep_resp;
  Vgamem_req_t vgamem_req;
  Vgamem_resp_t vgamem_resp;
  Reboot_req_t reboot_req;
  Reboot_resp_t reboot_resp;
  Unmask_msg_t unamsk_msg;
  Eoi_msg_t			eoi_msg;
  Gettime_req_t gettime_req;
  Gettime_resp_t gettime_resp;
  Umalloc_req_t umalloc_req;
  Umalloc_resp_t umalloc_resp;
  Pci_set_config_req_t pci_set_req;
  Pci_set_config_resp_t pci_set_resp;
  Poll_req_t poll_req;
  Poll_resp_t poll_resp;
  Get_core_req_t Get_core_req;
  Get_core_resp_t Get_core_resp;
  Ps_req_t ps_req;
  Ps_resp_t ps_resp;
  getstdio_req_t getstdio_req; /* stdio */
  getstdio_resp_t getstdio_resp;
  redirect_req_t redirect_req;
  redirect_resp_t redirect_resp;
#ifdef KERNEL_DEBUG
  kprintint_req_t kprintint_req;
  kprintint_resp_t kprintint_resp;
  kprintstr_req_t kprintstr_req;
  kprintstr_resp_t kprintstr_resp;
    kprintaddr_req_t kprintaddr_req;  
#endif
  Sigact_req_t sigact_req;
  Sigact_resp_t sigact_resp;
  Sigret_req_t sigret_req;
  /* no sigret response */
  
  Alarm_req_t alarm_req;
	Alarm_resp_t alarm_resp;

  /*user process memory messages */
  Map_dma_req_t dma_req;
  Map_dma_resp_t dma_resp;
  Map_mmio_req_t mmio_req;
  Map_mmio_resp_t mmio_resp;
  Msi_en_req_t    msi_req;
  Msi_en_resp_t    msi_resp;
   

} Systask_msg_t;

int getstr(char *s, int len);
void bsleep(int ms);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
// int ifconfig(char *interface, struct in_addr *ip, struct in_addr *nm, struct in_addr *gw);
uint16_t* map_vga_mem();
void reboot();
void unmask_irq(unsigned char irq);
int force_vmexit(uint64_t int1, uint64_t int2, void *strct_1);

void kprintint(char *strp,int val,int src);
void kprintstr(char *strp);

int getstdio(int val);
void redirect(int inpid,int outpid,int errpid);
