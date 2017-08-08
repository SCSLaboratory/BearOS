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

#define VPROC_SITE 0		 /* hypv/vproc.c */
#define HYPV_SITE 1		 /* hypv/hypv.c */
#define KERNEL_SITE 2		 /* kernel/kernel.c */
#define KVMEM_SITE 3		 /* kernel/kvmem.c */
#define SYS_TASK_SITE 4		 /* kernel/sys_task.c */
#define KWAIT_SITE 5		 /* kernel/kwait.c */
#define KMSG_SITE 6		 /* kernel/kmsg.c */
#define PROCMAN_SITE 7		 /* kernel/procman.c */
#define KTIMER_SITE 8		 /* utils/ktimer.c */
#define DIVERSITY_SITE 9	 /* utils/diversity.c */
#define REFRESH_SITE 10		 /* utils/refresh.c */
#define RANDOM_SITE 11		 /* utils/random.c */
#define INTERRUPTS_SITE 12	 /* utils/interrupts.c */
#define ACPI_SITE 13		 /* utils/acpi.c */
#define ELF_LOADER_SITE 14	 /* utils/elf_loader.c */
#define FILE_ABSTRACTION_SITE 15 /* utils/file_abstraction.c */
#define KQUEUE_SITE 16		 /* utils/kqueue.c */
#define VECTOR_SITE 17		 /* utils/vector.c */
#define VMEM_LAYER_SITE 18	 /* utils/vmem_layer.c */
#define PCI_SITE 19		 /* utils/pci.c */
#define KHASH_SITE 20		 /* utils/khash.c */
#define SEMAPHORE_SITE 21	 /* utils/semaphore.c */
#define PES_SITE 23      	 /* utils/pes.c */
#define KSCHED_SITE 24      	 /* utils/ksched.c */
#define KLOAD_SITE 25      	 /* kernel/kload.c */

#define NUMSITES 26


#ifdef KMALLOC_TRACKING

#define kmalloc_track(site,bytes)\
  kmalloc_track_site(site,bytes)
#define kfree_track(site,p)\
  kfree_track_site(site,p)

#else

#define kmalloc_track(site,bytes)\
  kmalloc(bytes)
#define kfree_track(site,p)\
  kfree(p)

#endif
