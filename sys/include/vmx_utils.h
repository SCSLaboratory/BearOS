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
#ifndef _IN_ASM
#include <vproc.h>

/* vmx support_test */
#define VMX_SUPPORT_SUCCESS 0
#define VMX_SUPPORT_ERROR_NO_VMX 1

/* vmx_lockout_test. */
#define VMX_LOCKOUT_SUCCESS 0
#define VMX_LOCKOUT_ERROR_LOCKED_OFF 1

/* vmx_register_test. */
#define VMX_REGISTER_SUCCESS 0
#define VMX_REGISTER_ERROR_BAD_CR0 1
#define VMX_REGISTER_ERROR_BAD_CR4 2

int vmxon(uint64_t vmxon_region);
int vmclear(uint64_t vmcs);
int vmlaunch();
int vmresume(); 
int vmptrld(uint64_t vmcs);
int vmptrst(uint64_t *vmcs);
int vmread(uint64_t field, uint64_t *value);
int vmwrite(uint64_t field, uint64_t value); 
int vmx_support_test();
int vmx_lockout_test(); 
/* Tests to make sure VMXON will go okay when issued. */
int vmx_register_test();

void add_to_memmap(vproc_t *vp, uint64_t base, uint64_t length, int type);

void print_gpregs(vproc_t *vp);

void hypv_acquire_lock();
void hypv_release_lock();

/*IPI information is limited. pass the pointer to the vp to join via global*/
vproc_t *SIPI_vp;

/*Each core uses one of these structures to store local registers on vmexit */
typedef struct vcpu_t{
  struct mcontext reg_storage;
  uint32_t assigned;
  uint64_t vmxon_region_virt;
  uint64_t stack;
} __attribute__ ((packed)) vcpu_t; 

vcpu_t *vcpu_get_last();

vcpu_t **vcpu_ptr_array;
#endif
