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

#define DIVERSITY_VMEM_SITE 0	/* sys/utils/diversity.c */
#define MEMORY_VMEM_SITE 1	/* sys/utils/memory.c    */
#define VMEM_LAYER_VMEM_SITE 2	/* sys/utils/vmem_layer.c*/
#define KVMEM_VMEM_SITE 3	/* sys/kernel/kvmem.c     */
#define SYS_TASK_VMEM_SITE 4	/* sys/kernel/sys_task.c  */
#define PROCMAN_VMEM_SITE 5	/* sys/kernel/procman.c   */
#define VPROC_VMEM_SITE 6	/* sys/hypv/vproc.c     */
#define PES_VMEM_SITE 7		/* sys/utils/pes.c     */
#define SMP_VMEM_SITE 8		/* sys/kernel/smp.c */

#define NUMVMEMSITES 9


#ifdef KVMEM_TRACKING

/* virtual memory tracking -- record the site */
#define vmem_alloc_region_track(site,bytes, pid)\
  vmem_alloc_region_track_site(site,bytes, pid)
#define vmem_free_region_track(site,p)\
  vmem_free_region_track_site(site,p)

#else

/* not tracking loose the site */
#define vmem_alloc_region_track(site,bytes, pid)\
  vmem_alloc_region(bytes, pid)
#define vmem_free_region_track(site,p)\
  vmem_free_region(p)

#endif
