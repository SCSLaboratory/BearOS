#pragma once
#ifndef _IN_ASM
#include <stdint.h>
#include <memory.h>
#include <mcontext.h>
#include <vector.h>
#include <vmexit.h>
#endif


/* Set the revision ID for VMCS */
#define revid(revid, vmcs) (*((uint64_t *)vmcs) = (uint32_t)revid)

/* ASM for status checking. The usual case is this is outputs 0 and 1. */
#define VM_STATUS_CHECK				\
  "setc %0\n\t"					\
  "setz %1\n\t"

#define VM_STATUS_CONSTRAINTS(invalid, valid) "=r"(invalid), "=r"(valid)

/* Check the result of a VMX region modification call. */
#define VM_STATUS_VALID 0
#define VM_STATUS_ERROR_INVALID 1
#define VM_STATUS_ERROR_VALID 2
#define VM_STATUS_RETURN(invalid, valid)			\
  return (invalid ? VM_STATUS_ERROR_INVALID :			\
	  (valid ? VM_STATUS_ERROR_VALID : VM_STATUS_VALID))

#define GUEST_INTERRUPT_STATE_STI       0x00000001
#define GUEST_INTERRUPT_STATE_MOV_SS    0x00000002
#define GUEST_INTERRUPT_STATE_SMI       0x00000004
#define GUEST_INTERRUPT_STATE_NMI       0x00000008
#define RFLAGS_IF                       0x00000200 /* Interrupt Flag */

/*pin based execution control fields */
#define PIN_EXT_INT_EXITING 		0
#define PIN_NMI_EXITING 	       	3
#define PIN_VIRTUAL_NMI 		5
#define PIN_ACTIVATE_PREEMPT_TIMER      6

/*primary procesor based exec control bit numbers */
#define PROC_PRI_INT_WINDOW_EXIT	2
#define PROC_PRI_USE_TSC_OFFSET	        3
#define PROC_PRI_HLT_EXITING		7
#define PROC_PRI_INVLPG_EXITING	        9
#define PROC_PRI_MWAIT_EXITING	        10
#define PROC_PRI_RDPMC_EXITING	        11
#define PROC_PRI_RDTSC_EXITING	        12
#define PROC_PRI_CR3_LOAD_EXIT		15
#define PROC_PRI_CR3_STORE_EXIT	        16
#define PROC_PRI_CR8_LOAD_EXIT		19
#define PROC_PRI_CR8_STORE_EXIT	        20
#define PROC_PRI_USE_TPR_SHADOW	        21
#define PROC_PRI_NMI_WIN_EXIT		22
#define PROC_PRI_MOV_DR			23
#define PROC_PRI_UNCOND_IO		24
#define PROC_PRI_USE_IO_BITMAPS	        25
#define PROC_PRI_MONITOR_TRAP_FL	27
#define PROC_PRI_USE_MSR_BITMAP	        28
#define PROC_PRI_MONITOR_EXITING	29
#define PROC_PRI_PAUSE_LOOP_EXIT	30
#define PROC_PRI_SECONDARY_CTRLS_ON     31

/* secondary procesor based exec control bit numbers */
#define PROC_SEC_VIRT_APIC_ACCESS       0
#define PROC_SEC_ENABLE_EPT             1
#define PROC_SEC_DESCRIPTOR_TABLE_EXIT  2
#define PROC_SEC_ENABLE_RDTSCP          3
#define PROC_SEC_VIRT_X2APIC_ACCESS     4
#define PROC_SEC_ENABLE_VPID            5
#define PROC_SEC_WBINVD_EXIT            6
#define PROC_SEC_UNRESTRICTED           7
#define PROC_SEC_APIC_REGISTER_VIRT     8
#define PROC_SEC_VIRT_INT_DELIVERY      9 


/* Set up the various VMCS stuff.
 * This is an enormous pain, because we can't have the C preprocessor generate
 * C preprocessor constructs. Thus, every definition has to be duplicated. */
#ifdef _IN_ASM
#define VIRTUAL_PROCESSOR_ID          0x00000000
#define GUEST_ES_SELECTOR             0x00000800
#define GUEST_CS_SELECTOR             0x00000802
#define GUEST_SS_SELECTOR             0x00000804
#define GUEST_DS_SELECTOR             0x00000806
#define GUEST_FS_SELECTOR             0x00000808
#define GUEST_GS_SELECTOR             0x0000080a
#define GUEST_LDTR_SELECTOR           0x0000080c
#define GUEST_TR_SELECTOR             0x0000080e
#define HOST_ES_SELECTOR              0x00000c00
#define HOST_CS_SELECTOR              0x00000c02
#define HOST_SS_SELECTOR              0x00000c04
#define HOST_DS_SELECTOR              0x00000c06
#define HOST_FS_SELECTOR              0x00000c08
#define HOST_GS_SELECTOR              0x00000c0a
#define HOST_TR_SELECTOR              0x00000c0c
#define IO_BITMAP_A                   0x00002000
#define IO_BITMAP_A_HIGH              0x00002001
#define IO_BITMAP_B                   0x00002002
#define IO_BITMAP_B_HIGH              0x00002003
#define MSR_BITMAP                    0x00002004
#define MSR_BITMAP_HIGH               0x00002005
#define VM_EXIT_MSR_STORE_ADDR        0x00002006
#define VM_EXIT_MSR_STORE_ADDR_HIGH   0x00002007
#define VM_EXIT_MSR_LOAD_ADDR         0x00002008
#define VM_EXIT_MSR_LOAD_ADDR_HIGH    0x00002009
#define VM_ENTRY_MSR_LOAD_ADDR        0x0000200a
#define VM_ENTRY_MSR_LOAD_ADDR_HIGH   0x0000200b
#define TSC_OFFSET                    0x00002010
#define TSC_OFFSET_HIGH               0x00002011
#define VIRTUAL_APIC_PAGE_ADDR        0x00002012
#define VIRTUAL_APIC_PAGE_ADDR_HIGH   0x00002013
#define APIC_ACCESS_ADDR              0x00002014
#define APIC_ACCESS_ADDR_HIGH         0x00002015
#define EPT_POINTER                   0x0000201a
#define EPT_POINTER_HIGH              0x0000201b
#define GUEST_PHYSICAL_ADDRESS        0x00002400
#define GUEST_PHYSICAL_ADDRESS_HIGH   0x00002401
#define VMCS_LINK_POINTER             0x00002800
#define VMCS_LINK_POINTER_HIGH        0x00002801
#define GUEST_IA32_DEBUGCTL           0x00002802
#define GUEST_IA32_DEBUGCTL_HIGH      0x00002803
#define GUEST_PAT                     0x00002804
#define GUEST_PAT_HIGH                0x00002805
#define GUEST_EFER                    0x00002806
#define GUEST_EFER_HIGH               0x00002807
#define GUEST_PDPTR0                  0x0000280a
#define GUEST_PDPTR0_HIGH             0x0000280b
#define GUEST_PDPTR1                  0x0000280c
#define GUEST_PDPTR1_HIGH             0x0000280d
#define GUEST_PDPTR2                  0x0000280e
#define GUEST_PDPTR2_HIGH             0x0000280f
#define GUEST_PDPTR3                  0x00002810
#define GUEST_PDPTR3_HIGH             0x00002811
#define HOST_PAT                      0x00002c00
#define HOST_PAT_HIGH                 0x00002c01
#define HOST_EFER                     0x00002c02
#define HOST_EFER_HIGH                0x00002c03
#define PIN_BASED_VM_EXEC_CONTROL     0x00004000
#define CPU_BASED_VM_EXEC_CONTROL     0x00004002
#define EXCEPTION_BITMAP              0x00004004
#define PAGE_FAULT_ERROR_CODE_MASK    0x00004006
#define PAGE_FAULT_ERROR_CODE_MATCH   0x00004008
#define CR3_TARGET_COUNT              0x0000400a
#define VM_EXIT_CONTROLS              0x0000400c
#define VM_EXIT_MSR_STORE_COUNT       0x0000400e
#define VM_EXIT_MSR_LOAD_COUNT        0x00004010
#define VM_ENTRY_CONTROLS             0x00004012
#define VM_ENTRY_MSR_LOAD_COUNT       0x00004014
#define VM_ENTRY_INTR_INFO            0x00004016
#define VM_ENTRY_EXCEPTION_ERROR_CODE 0x00004018
#define VM_ENTRY_INSTRUCTION_LEN      0x0000401a
#define TPR_THRESHOLD                 0x0000401c
#define SECONDARY_VM_EXEC_CONTROL     0x0000401e
#define PLE_GAP                       0x00004020
#define PLE_WINDOW                    0x00004022
#define VM_INSTRUCTION_ERROR          0x00004400
#define VM_EXIT_REASON                0x00004402
#define VM_EXIT_INTR_INFO             0x00004404
#define VM_EXIT_INTR_ERROR_CODE       0x00004406
#define IDT_VECTORING_INFO            0x00004408
#define IDT_VECTORING_ERROR_CODE      0x0000440a
#define VM_EXIT_INSTRUCTION_LEN       0x0000440c
#define VMX_INSTRUCTION_INFO          0x0000440e
#define GUEST_ES_LIMIT                0x00004800
#define GUEST_CS_LIMIT                0x00004802
#define GUEST_SS_LIMIT                0x00004804
#define GUEST_DS_LIMIT                0x00004806
#define GUEST_FS_LIMIT                0x00004808
#define GUEST_GS_LIMIT                0x0000480a
#define GUEST_LDTR_LIMIT              0x0000480c
#define GUEST_TR_LIMIT                0x0000480e
#define GUEST_GDTR_LIMIT              0x00004810
#define GUEST_IDTR_LIMIT              0x00004812
#define GUEST_ES_AR_BYTES             0x00004814
#define GUEST_CS_AR_BYTES             0x00004816
#define GUEST_SS_AR_BYTES             0x00004818
#define GUEST_DS_AR_BYTES             0x0000481a
#define GUEST_FS_AR_BYTES             0x0000481c
#define GUEST_GS_AR_BYTES             0x0000481e
#define GUEST_LDTR_AR_BYTES           0x00004820
#define GUEST_TR_AR_BYTES             0x00004822
#define GUEST_INTERRUPTIBILITY_INFO   0x00004824
#define GUEST_ACTIVITY_STATE          0x00004826
#define GUEST_SYSENTER_CS             0x0000482a
#define VMX_PREEMPTION_TIMER_VALUE    0x0000482e
#define HOST_SYSENTER_CS              0x00004c00
#define CR0_GUEST_HOST_MASK           0x00006000
#define CR4_GUEST_HOST_MASK           0x00006002
#define CR0_READ_SHADOW               0x00006004
#define CR4_READ_SHADOW               0x00006006
#define CR3_TARGET_VALUE0             0x00006008
#define CR3_TARGET_VALUE1             0x0000600a
#define CR3_TARGET_VALUE2             0x0000600c
#define CR3_TARGET_VALUE3             0x0000600e
#define EXIT_QUALIFICATION            0x00006400
#define GUEST_LINEAR_ADDRESS          0x0000640a
#define GUEST_CR0                     0x00006800
#define GUEST_CR3                     0x00006802
#define GUEST_CR4                     0x00006804
#define GUEST_ES_BASE                 0x00006806
#define GUEST_CS_BASE                 0x00006808
#define GUEST_SS_BASE                 0x0000680a
#define GUEST_DS_BASE                 0x0000680c
#define GUEST_FS_BASE                 0x0000680e
#define GUEST_GS_BASE                 0x00006810
#define GUEST_LDTR_BASE               0x00006812
#define GUEST_TR_BASE                 0x00006814
#define GUEST_GDTR_BASE               0x00006816
#define GUEST_IDTR_BASE               0x00006818
#define GUEST_DR7                     0x0000681a
#define GUEST_RSP                     0x0000681c
#define GUEST_RIP                     0x0000681e
#define GUEST_RFLAGS                  0x00006820
#define GUEST_PENDING_DBG_EXCEPTIONS  0x00006822
#define GUEST_SYSENTER_ESP            0x00006824
#define GUEST_SYSENTER_EIP            0x00006826
#define HOST_CR0                      0x00006c00
#define HOST_CR3                      0x00006c02
#define HOST_CR4                      0x00006c04
#define HOST_FS_BASE                  0x00006c06
#define HOST_GS_BASE                  0x00006c08
#define HOST_TR_BASE                  0x00006c0a
#define HOST_GDTR_BASE                0x00006c0c
#define HOST_IDTR_BASE                0x00006c0e
#define HOST_SYSENTER_ESP             0x00006c10
#define HOST_SYSENTER_EIP             0x00006c12
#define HOST_RSP                      0x00006c14
#define HOST_RIP                      0x00006c16
#else
enum vmcs_field {
  VIRTUAL_PROCESSOR_ID            = 0x00000000,
  GUEST_ES_SELECTOR               = 0x00000800,
  GUEST_CS_SELECTOR               = 0x00000802,
  GUEST_SS_SELECTOR               = 0x00000804,
  GUEST_DS_SELECTOR               = 0x00000806,
  GUEST_FS_SELECTOR               = 0x00000808,
  GUEST_GS_SELECTOR               = 0x0000080a,
  GUEST_LDTR_SELECTOR             = 0x0000080c,
  GUEST_TR_SELECTOR               = 0x0000080e,
  HOST_ES_SELECTOR                = 0x00000c00,
  HOST_CS_SELECTOR                = 0x00000c02,
  HOST_SS_SELECTOR                = 0x00000c04,
  HOST_DS_SELECTOR                = 0x00000c06,
  HOST_FS_SELECTOR                = 0x00000c08,
  HOST_GS_SELECTOR                = 0x00000c0a,
  HOST_TR_SELECTOR                = 0x00000c0c,
  IO_BITMAP_A                     = 0x00002000,
  IO_BITMAP_A_HIGH                = 0x00002001,
  IO_BITMAP_B                     = 0x00002002,
  IO_BITMAP_B_HIGH                = 0x00002003,
  MSR_BITMAP                      = 0x00002004,
  MSR_BITMAP_HIGH                 = 0x00002005,
  VM_EXIT_MSR_STORE_ADDR          = 0x00002006,
  VM_EXIT_MSR_STORE_ADDR_HIGH     = 0x00002007,
  VM_EXIT_MSR_LOAD_ADDR           = 0x00002008,
  VM_EXIT_MSR_LOAD_ADDR_HIGH      = 0x00002009,
  VM_ENTRY_MSR_LOAD_ADDR          = 0x0000200a,
  VM_ENTRY_MSR_LOAD_ADDR_HIGH     = 0x0000200b,
  TSC_OFFSET                      = 0x00002010,
  TSC_OFFSET_HIGH                 = 0x00002011,
  VIRTUAL_APIC_PAGE_ADDR          = 0x00002012,
  VIRTUAL_APIC_PAGE_ADDR_HIGH     = 0x00002013,
  APIC_ACCESS_ADDR                = 0x00002014,
  APIC_ACCESS_ADDR_HIGH           = 0x00002015,
  EPT_POINTER                     = 0x0000201a,
  EPT_POINTER_HIGH                = 0x0000201b,
  GUEST_PHYSICAL_ADDRESS          = 0x00002400,
  GUEST_PHYSICAL_ADDRESS_HIGH     = 0x00002401,
  VMCS_LINK_POINTER               = 0x00002800,
  VMCS_LINK_POINTER_HIGH          = 0x00002801,
  GUEST_IA32_DEBUGCTL             = 0x00002802,
  GUEST_IA32_DEBUGCTL_HIGH        = 0x00002803,
  GUEST_PAT                       = 0x00002804,
  GUEST_PAT_HIGH                  = 0x00002805,
  GUEST_EFER                      = 0x00002806,
  GUEST_EFER_HIGH                 = 0x00002807,
  GUEST_PDPTR0                    = 0x0000280a,
  GUEST_PDPTR0_HIGH               = 0x0000280b,
  GUEST_PDPTR1                    = 0x0000280c,
  GUEST_PDPTR1_HIGH               = 0x0000280d,
  GUEST_PDPTR2                    = 0x0000280e,
  GUEST_PDPTR2_HIGH               = 0x0000280f,
  GUEST_PDPTR3                    = 0x00002810,
  GUEST_PDPTR3_HIGH               = 0x00002811,
  HOST_PAT                        = 0x00002c00,
  HOST_PAT_HIGH                   = 0x00002c01,
  HOST_EFER                       = 0x00002c02,
  HOST_EFER_HIGH                  = 0x00002c03,
  PIN_BASED_VM_EXEC_CONTROL       = 0x00004000,
  CPU_BASED_VM_EXEC_CONTROL       = 0x00004002,
  EXCEPTION_BITMAP                = 0x00004004,
  PAGE_FAULT_ERROR_CODE_MASK      = 0x00004006,
  PAGE_FAULT_ERROR_CODE_MATCH     = 0x00004008,
  CR3_TARGET_COUNT                = 0x0000400a,
  VM_EXIT_CONTROLS                = 0x0000400c,
  VM_EXIT_MSR_STORE_COUNT         = 0x0000400e,
  VM_EXIT_MSR_LOAD_COUNT          = 0x00004010,
  VM_ENTRY_CONTROLS               = 0x00004012,
  VM_ENTRY_MSR_LOAD_COUNT         = 0x00004014,
  VM_ENTRY_INTR_INFO              = 0x00004016,
  VM_ENTRY_EXCEPTION_ERROR_CODE   = 0x00004018,
  VM_ENTRY_INSTRUCTION_LEN        = 0x0000401a,
  TPR_THRESHOLD                   = 0x0000401c,
  SECONDARY_VM_EXEC_CONTROL       = 0x0000401e,
  PLE_GAP                         = 0x00004020,
  PLE_WINDOW                      = 0x00004022,
  VM_INSTRUCTION_ERROR            = 0x00004400,
  VM_EXIT_REASON                  = 0x00004402,
  VM_EXIT_INTR_INFO               = 0x00004404,
  VM_EXIT_INTR_ERROR_CODE         = 0x00004406,
  IDT_VECTORING_INFO              = 0x00004408,
  IDT_VECTORING_ERROR_CODE        = 0x0000440a,
  VM_EXIT_INSTRUCTION_LEN         = 0x0000440c,
  VMX_INSTRUCTION_INFO            = 0x0000440e,
  GUEST_ES_LIMIT                  = 0x00004800,
  GUEST_CS_LIMIT                  = 0x00004802,
  GUEST_SS_LIMIT                  = 0x00004804,
  GUEST_DS_LIMIT                  = 0x00004806,
  GUEST_FS_LIMIT                  = 0x00004808,
  GUEST_GS_LIMIT                  = 0x0000480a,
  GUEST_LDTR_LIMIT                = 0x0000480c,
  GUEST_TR_LIMIT                  = 0x0000480e,
  GUEST_GDTR_LIMIT                = 0x00004810,
  GUEST_IDTR_LIMIT                = 0x00004812,
  GUEST_ES_AR_BYTES               = 0x00004814,
  GUEST_CS_AR_BYTES               = 0x00004816,
  GUEST_SS_AR_BYTES               = 0x00004818,
  GUEST_DS_AR_BYTES               = 0x0000481a,
  GUEST_FS_AR_BYTES               = 0x0000481c,
  GUEST_GS_AR_BYTES               = 0x0000481e,
  GUEST_LDTR_AR_BYTES             = 0x00004820,
  GUEST_TR_AR_BYTES               = 0x00004822,
  GUEST_INTERRUPTIBILITY_INFO     = 0x00004824,
  GUEST_ACTIVITY_STATE            = 0x00004826,
  GUEST_SYSENTER_CS               = 0x0000482a,
  VMX_PREEMPTION_TIMER_VALUE      = 0x0000482e,
  HOST_SYSENTER_CS                = 0x00004c00,
  CR0_GUEST_HOST_MASK             = 0x00006000,
  CR4_GUEST_HOST_MASK             = 0x00006002,
  CR0_READ_SHADOW                 = 0x00006004,
  CR4_READ_SHADOW                 = 0x00006006,
  CR3_TARGET_VALUE0               = 0x00006008,
  CR3_TARGET_VALUE1               = 0x0000600a,
  CR3_TARGET_VALUE2               = 0x0000600c,
  CR3_TARGET_VALUE3               = 0x0000600e,
  EXIT_QUALIFICATION              = 0x00006400,
  GUEST_LINEAR_ADDRESS            = 0x0000640a,
  GUEST_CR0                       = 0x00006800,
  GUEST_CR3                       = 0x00006802,
  GUEST_CR4                       = 0x00006804,
  GUEST_ES_BASE                   = 0x00006806,
  GUEST_CS_BASE                   = 0x00006808,
  GUEST_SS_BASE                   = 0x0000680a,
  GUEST_DS_BASE                   = 0x0000680c,
  GUEST_FS_BASE                   = 0x0000680e,
  GUEST_GS_BASE                   = 0x00006810,
  GUEST_LDTR_BASE                 = 0x00006812,
  GUEST_TR_BASE                   = 0x00006814,
  GUEST_GDTR_BASE                 = 0x00006816,
  GUEST_IDTR_BASE                 = 0x00006818,
  GUEST_DR7                       = 0x0000681a,
  GUEST_RSP                       = 0x0000681c,
  GUEST_RIP                       = 0x0000681e,
  GUEST_RFLAGS                    = 0x00006820,
  GUEST_PENDING_DBG_EXCEPTIONS    = 0x00006822,
  GUEST_SYSENTER_ESP              = 0x00006824,
  GUEST_SYSENTER_EIP              = 0x00006826,
  HOST_CR0                        = 0x00006c00,
  HOST_CR3                        = 0x00006c02,
  HOST_CR4                        = 0x00006c04,
  HOST_FS_BASE                    = 0x00006c06,
  HOST_GS_BASE                    = 0x00006c08,
  HOST_TR_BASE                    = 0x00006c0a,
  HOST_GDTR_BASE                  = 0x00006c0c,
  HOST_IDTR_BASE                  = 0x00006c0e,
  HOST_SYSENTER_ESP               = 0x00006c10,
  HOST_SYSENTER_EIP               = 0x00006c12,
  HOST_RSP                        = 0x00006c14,
  HOST_RIP                        = 0x00006c16,
};
#endif

/* Example code for vmcs mods  */
#if 0
struct vmcs_mod *mod = kmalloc(sizeof(struct vmcs_mod));
mod->field = GUEST_RIP;
mod->value = NEW_VALUE;
qput(((vproc_t *)proc)->vmcs_mods, mod);
/* don't forget to update the local copy in the vmcs */
((vproc_t *)proc)->reg_storage.rip = mod->value;
#endif


#ifndef _IN_ASM
typedef struct vmcs {
  uint64_t PAGE_FAULT_ERROR_CODE_MASK;
  uint64_t PAGE_FAULT_ERROR_CODE_MATCH;

  /* VM exit information fields */
  uint64_t VM_EXIT_REASON;
  uint64_t EXIT_QUALIFICATION;

  uint64_t GUEST_LINEAR_ADDRESS;
  uint64_t GUEST_PHYSICAL_ADDRESS;
  uint64_t GUEST_PHYSICAL_ADDRESS_HIGH;

  uint64_t VM_EXIT_INTR_INFO;
  uint64_t VM_EXIT_INTR_ERROR_CODE;

  uint64_t IDT_VECTORING_INFO;
  uint64_t IDT_VECTORING_ERROR_CODE;
  uint64_t VM_EXIT_INSTRUCTION_LEN;
  uint64_t VMX_INSTRUCTION_INFO;	
	
  uint64_t VM_INSTRUCTION_ERROR;

  /* VM entry control fields */
  uint32_t VM_ENTRY_CONTROLS;	
	
  uint64_t VM_ENTRY_MSR_LOAD_COUNT;
  uint64_t VM_ENTRY_MSR_LOAD_ADDR;
  uint64_t VM_ENTRY_MSR_LOAD_ADDR_HIGH;	

  uint64_t VM_ENTRY_INTR_INFO;
  uint64_t VM_ENTRY_EXCEPTION_ERROR_CODE;
  uint64_t VM_ENTRY_INSTRUCTION_LEN;

  /* VM exit control fields */
  uint32_t VM_EXIT_CONTROLS;  
	
  uint64_t VM_EXIT_MSR_STORE_COUNT;
  uint64_t VM_EXIT_MSR_STORE_ADDR;
  uint64_t VM_EXIT_MSR_STORE_ADDR_HIGH;

  uint64_t VM_EXIT_MSR_LOAD_COUNT;
  uint64_t VM_EXIT_MSR_LOAD_ADDR;
  uint64_t VM_EXIT_MSR_LOAD_ADDR_HIGH;
	
	

  /* Execution control fields */
  uint32_t PIN_BASED_VM_EXEC_CONTROL;
  /* 21-14 vol3b.
   * See section 21.6.4, 22.1.3 for info on bit 25 use of IO bitmaps. */
  uint32_t CPU_BASED_VM_EXEC_CONTROL;
  uint64_t SECONDARY_VM_EXEC_CONTROL;
  uint64_t EXCEPTION_BITMAP;
	
  /* These control access to ports.
   * A controls 0000-7FFFH, B controls 8000-FFFF.
   * Setting a field to one causes a VM exit.
   * Enabled only if bit 25=1 in exec control
   */
  uint64_t IO_BITMAP_A;
  uint64_t IO_BITMAP_A_HIGH;
  uint64_t IO_BITMAP_B;
  uint64_t IO_BITMAP_B_HIGH;

  uint64_t TSC_OFFSET;
  uint64_t TSC_OFFSET_HIGH;

  uint64_t CR0_GUEST_HOST_MASK;
  uint64_t CR4_GUEST_HOST_MASK;
  uint64_t CR0_READ_SHADOW;
  uint64_t CR4_READ_SHADOW;
  uint64_t CR3_TARGET_VALUE0;
  uint64_t CR3_TARGET_VALUE1;
  uint64_t CR3_TARGET_VALUE2;
  uint64_t CR3_TARGET_VALUE3;
  uint64_t CR3_TARGET_COUNT;

  uint64_t VIRTUAL_APIC_PAGE_ADDR;
  uint64_t VIRTUAL_APIC_PAGE_ADDR_HIGH;
  uint64_t APIC_ACCESS_ADDR;
  uint64_t APIC_ACCESS_ADDR_HIGH;
  uint64_t TPR_THRESHOLD;
	
  //okay so where's read bitmap and write bitmap should be a low and high for 
  // each, unless we disable it or set it in processor execution control
  uint64_t MSR_BITMAP;
  uint64_t MSR_BITMAP_HIGH;
	
  /* Base of the PML4 table. */
  uint64_t EPT_POINTER;
  uint32_t EPT_POINTER_HIGH;

  /* See section 25.1 for details on this field. */
  uint64_t VIRTUAL_PROCESSOR_ID;  
	
  /* These set the bounds on how long a guest can sit in a pause loop. */
  uint64_t PLE_GAP;
  uint64_t PLE_WINDOW;

  /* Guest state area. */
  uint64_t GUEST_CR0;
  uint64_t GUEST_CR3;
  uint64_t GUEST_CR4;

  uint64_t GUEST_DR7;
  uint64_t GUEST_RSP;
  uint64_t GUEST_RIP;
  uint64_t GUEST_RFLAGS;

  uint64_t GUEST_ES_SELECTOR;
  uint64_t GUEST_ES_BASE;
  uint64_t GUEST_ES_LIMIT;
  uint64_t GUEST_ES_AR_BYTES;

  uint64_t GUEST_CS_SELECTOR;
  uint64_t GUEST_CS_BASE;
  uint64_t GUEST_CS_LIMIT;
  uint64_t GUEST_CS_AR_BYTES;

  uint64_t GUEST_SS_SELECTOR;
  uint64_t GUEST_SS_BASE;
  uint64_t GUEST_SS_LIMIT;
  uint64_t GUEST_SS_AR_BYTES;

  uint64_t GUEST_DS_SELECTOR;
  uint64_t GUEST_DS_BASE;
  uint64_t GUEST_DS_LIMIT;
  uint64_t GUEST_DS_AR_BYTES;

  uint64_t GUEST_FS_SELECTOR;
  uint64_t GUEST_FS_BASE;
  uint64_t GUEST_FS_LIMIT;
  uint64_t GUEST_FS_AR_BYTES;

  uint64_t GUEST_GS_SELECTOR;
  uint64_t GUEST_GS_BASE;
  uint64_t GUEST_GS_LIMIT;
  uint64_t GUEST_GS_AR_BYTES;

  uint64_t GUEST_LDTR_SELECTOR;
  uint64_t GUEST_LDTR_BASE;
  uint64_t GUEST_LDTR_LIMIT;
  uint64_t GUEST_LDTR_AR_BYTES;

  uint64_t GUEST_TR_SELECTOR;
  uint64_t GUEST_TR_BASE;	
  uint64_t GUEST_TR_LIMIT;
  uint64_t GUEST_TR_AR_BYTES;

  uint64_t GUEST_GDTR_BASE;
  uint64_t GUEST_IDTR_BASE;
  uint64_t GUEST_GDTR_LIMIT;
  uint64_t GUEST_IDTR_LIMIT;

  uint64_t GUEST_IA32_DEBUGCTL;
  uint64_t GUEST_IA32_DEBUGCTL_HIGH;
  uint64_t GUEST_SYSENTER_CS;
  uint32_t VMX_PREEMPTION_TIMER_VALUE;
  uint64_t GUEST_SYSENTER_ESP;
  uint64_t GUEST_SYSENTER_EIP;

  uint64_t GUEST_PAT;
  uint64_t GUEST_PAT_HIGH;
  uint64_t GUEST_EFER;
  uint64_t GUEST_EFER_HIGH;

  /* Non-register guest state. */
  uint64_t GUEST_ACTIVITY_STATE;
  uint64_t GUEST_INTERRUPTIBILITY_INFO;
  uint64_t GUEST_PENDING_DBG_EXCEPTIONS;
  uint64_t VMCS_LINK_POINTER;
  uint64_t VMCS_LINK_POINTER_HIGH;
  //These fields correspond to the pagedire pte referenced by CR3 when pagign is in use
  uint64_t GUEST_PDPTR0;
  uint64_t GUEST_PDPTR0_HIGH;
  uint64_t GUEST_PDPTR1;
  uint64_t GUEST_PDPTR1_HIGH;
  uint64_t GUEST_PDPTR2;
  uint64_t GUEST_PDPTR2_HIGH;
  uint64_t GUEST_PDPTR3;
  uint64_t GUEST_PDPTR3_HIGH;

  /* Host state area. */
  uint64_t HOST_SYSENTER_CS;
  uint64_t HOST_PAT;
  uint64_t HOST_PAT_HIGH;
  uint64_t HOST_EFER;
  uint64_t HOST_EFER_HIGH;
  uint64_t HOST_ES_SELECTOR;
  uint64_t HOST_CS_SELECTOR;
  uint64_t HOST_SS_SELECTOR;
  uint64_t HOST_DS_SELECTOR;
  uint64_t HOST_FS_SELECTOR;
  uint64_t HOST_GS_SELECTOR;
  uint64_t HOST_TR_SELECTOR;
  uint64_t HOST_CR0;
  uint64_t HOST_CR3;
  uint64_t HOST_CR4;
  uint64_t HOST_FS_BASE;
  uint64_t HOST_GS_BASE;
  uint64_t HOST_TR_BASE;
  uint64_t HOST_GDTR_BASE;
  uint64_t HOST_IDTR_BASE;
  uint64_t HOST_SYSENTER_ESP;
  uint64_t HOST_SYSENTER_EIP;
  uint64_t HOST_RSP;
  uint64_t HOST_RIP;
} vmcs_t;
extern struct vmcs vmcs_default;

/* Prefixed with MSR to get around the #define's */
struct vmx_msrs_t {
  uint64_t MSR_IA32_FEATURE_CONTROL;
  uint64_t MSR_IA32_VMX_BASIC;
  uint64_t MSR_IA32_VMX_PINBASED_CTLS;
  uint64_t MSR_IA32_VMX_PROCBASED_CTLS;
  uint64_t MSR_IA32_VMX_EXIT_CTLS;
  uint64_t MSR_IA32_VMX_ENTRY_CTLS;
  uint64_t MSR_IA32_VMX_MISC;
  uint64_t MSR_IA32_VMX_CR0_FIXED0;
  uint64_t MSR_IA32_VMX_CR0_FIXED1;
  uint64_t MSR_IA32_VMX_CR4_FIXED0;
  uint64_t MSR_IA32_VMX_CR4_FIXED1;
  uint64_t MSR_IA32_VMX_PROCBASED_CTLS2;
  uint64_t MSR_IA32_VMX_TRUE_PINBASED_CTLS;
  uint64_t MSR_IA32_VMX_TRUE_PROCBASED_CTLS;
  uint64_t MSR_IA32_VMX_TRUE_EXIT_CTLS;
  uint64_t MSR_IA32_VMX_TRUE_ENTRY_CTLS;
  uint64_t MSR_IA32_VMX_VMFUNC;
} vmx_msrs;

struct vmcs_mod {
  enum vmcs_field field;
  uint64_t value;
};

typedef struct waiting_interrupts_t{
  int vector; 
} waiting_interrupt_t;

void hypv_entry(uint64_t );
void init_vmcs_defaults();
void write_vmcs(vmcs_t *vmcs);
void hypv_map_bce_mmio(void *varg, void *vdev);
void inject_event( uint16_t vector, uint8_t type);


uint32_t guest_interruptable();
void clear_bit(uint64_t * field, int position);

int vmclear(uint64_t);
int vmptrld(uint64_t);

int vmread(uint64_t field, uint64_t *value);
int vmwrite(uint64_t field, uint64_t value);

/*move me*/
void map_dma_to_guest(void *vp);
#endif /*_IN_ASM*/

#if defined(HYPV_SHIM) && defined(KERNEL)
void shim_trap();
#endif
