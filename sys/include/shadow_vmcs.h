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

/* This structure contains all of the fields a guest would creatw if
   it were a hypervisor. Instead the acutal hypervisor will keep track
   of these fields for the guest. */
typedef struct shadow_vmcs{
  uint64_t guest_vmxon_region_phys; /* guest physical address of vmxon region*/
  uint64_t host_vmxon_region_phys;  /* host physical address of vmxon region */
  uint64_t vm_revid;                /* revision ID written into vmxon region */
  uint64_t guest_vmcs_ptr_phys;     /* guest physical address of vmcsptr phys*/
  uint64_t host_vmcs_ptr_phys;      /* host physcial address of vmcsptr phys */
  void*    host_vmcs_ptr;           /* host virtual address of vmcsptr       */

}shadow_vmcs_t;
