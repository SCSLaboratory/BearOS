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

/* elf.c -- a very basic ELF loader to read and load the hypervisor.
 * The hypervisor and kernel have a way more comprehensive ELF loader; this one
 * is as basic as it can get.
 *
 * TODO: All verification.
 *       Make sure that no symbols have SHN_UNDEF.
 */
#include <constants.h>
#include <elf.h>
#include <ramio.h>
#include <fatfs.h>
#include <kstdio.h>
#include <kstring.h>
#include <asm_subroutines.h> 
#include <bootmemory.h>
#include <memory.h>

static unsigned int elf_permission_convert(Elf64_Word);

/* Check the values in the header, make sure they're correct for what our 
   simple bootloader ELF reader can handle. */
static int checkhdr(struct Elf64_Ehdr *hdr) {
  /* Check for the magic number */
  if(hdr->e_ident[EL_MAG0] !='\x7f' ||
     hdr->e_ident[EL_MAG1] != 'E'   ||
     hdr->e_ident[EL_MAG2] != 'L'   ||
     hdr->e_ident[EL_MAG3] != 'F')
    return 1;
  /* Make sure we're class 2 (ELF64) */
  if(hdr->e_ident[EL_CLASS] != ELFCLASS64)
    return 2;
  /* Make sure we're little-endian (we can patch this up later, a proper
   * loader would do so) */
  if(hdr->e_ident[EL_DATA] != ELFDATA2LSB)
    return 3;
  /* Make sure we're using the SYSV ABI */
  if(hdr->e_ident[EL_OSABI] != ELFOSABI_SYSV)
    return 4;
  /* Make sure we're an executable file and only an executable file */
  if(hdr->e_type != ET_EXEC)
    return 5;

  return 0;
}
/* Invert the above function into a "valid or not" question. */
#define valid_header(x) (checkhdr(x) ? 0 : 1)

static int checkphdr(struct Elf64_Phdr *hdr) {
  /* We don't have a comprehensive enough loader here to deal with segments 
     that aren't PT_LOAD segments (including unwind and whatnot). Rather than 
     just ignoring them, which is probably not a bad way to deal with the 
     problem, we'll just fail here. When linking the hypervisor, make sure 
     that these sections are discarded. Eventually we'll find a way to deal 
     with them (even if it's just ignoring them). */
  /* One of those control fake headers that we don't care about? */
#ifdef DEBUG
  kprintf( "[Boot] Header p_memsz: %x, p_type %x, p_vaddr %x\n", hdr->p_memsz, hdr->p_type, hdr->p_vaddr);
#endif
  if(hdr->p_memsz == 0)
    goto out;

  if(hdr->p_type != PT_LOAD)
    return 1;

  /* Make sure the hypervisor is to be loaded above 2 MB! */
  if(hdr->p_vaddr < 0x200000)
    return 2;

 out:
  return 0;
}
/* Invert the above function into a "valid or not" question. */
#define valid_pheader(x) (checkphdr(x) ? 0 : 1)

static int load_segment(struct Elf64_Phdr *hdr, FIL *file) {
  unsigned int fstatus;
  unsigned int flags;
  int rc;

  /* Extract the read-write-execute bits for setting the pages. */
  flags = elf_permission_convert(hdr->p_flags);
  if((!(flags & PERM_READ)) || (!(flags & PERM_EXEC)) || (flags & PERM_WRITE))
    {
#ifdef DEBUG /*This is happening, doesn't seem to be a problem*/
      kprintf("[Boot] ELF permissions inccorect on segment! %d, %d, %d\n", flags & PERM_READ, flags & PERM_EXEC, flags & PERM_WRITE);
#endif
      //panic();
    }
  /* Get to the necessary point in the file to load this segment. */
  fstatus = f_lseek(file, hdr->p_offset);
  if(fstatus != FR_OK)
    {
      kprintf("[Boot] Error while lseek file segment\n");
      //panic();
    }

  kprintf("hdr->p_vaddr = 0x%x and %d\n", hdr->p_vaddr, hdr->p_filesz);
  page_kernel(hdr->p_vaddr, hdr->p_memsz);

  /* store information about the kernel load location */
  *(uint64_t*)KERNEL_BEGIN_LOCATION = hdr->p_vaddr;
  *(uint64_t*)KERNEL_END_LOCATION   = hdr->p_vaddr + hdr->p_memsz;

  /* Copy the data from the file into RAM. */
  rc = f_read(file, (void *)(hdr->p_vaddr), hdr->p_memsz, &fstatus);
  kprintf("Loading kernel to vaddr %x \n", (void *)(hdr->p_vaddr));
  if(rc != FR_OK)
    {
      kprintf("[Boot] Error loading segment in bootloader\n");
      //panic();
    }
	
  /* Zero any extra memory, as per the standard. */
  if(hdr->p_memsz > hdr->p_filesz)
    kmemset((void *)(hdr->p_vaddr + hdr->p_filesz), 0, hdr->p_memsz - hdr->p_filesz);

  kputs("returning from load_segment");

  return 0;
}

/* This function has a MAJOR side effect on the system memory! It will pull the
 * contents of the ELF file into memory where the ELF tables specify it should
 * be, and clobber anything that's there. Be careful!
 */
#define checkstatus(x) if(x) return 1
int read(FIL *file,                          /* File handle to read from */
         FILINFO *stat,                      /* f_stat() results for file */
         uint64_t *entry) {                  /* Entry point returned in this */
  struct Elf64_Ehdr file_header;
  struct Elf64_Phdr program_header;
  unsigned int fstatus;
  int i;
  int rc;
  int check;
  int seekto;

  /* The header */
  rc = f_read(file, &file_header, sizeof(struct Elf64_Ehdr), &fstatus);
  checkstatus(rc);
  check = valid_header(&file_header);
  if(!check) {
    return 1;
  }

  /* Read in the program headers and, as each header is read, that section 
     into memory. Major side effect here on memory, and we make sure that OUR 
     stack is kept out of the way so we don't clobber ourselves! No guarantees 
     that we don't clobber our code though, so what we really do is have the 
     hypervisor loaded at 2TB -- we're pretty much guaranteed to not be at or 
     above that point. */
  seekto = file_header.e_phoff;
  for(i = 0; i < file_header.e_phnum; i++) {
    fstatus = f_lseek(file, seekto);
    rc = f_read(file, &program_header, sizeof(struct Elf64_Phdr), &fstatus);
    checkstatus(rc);
    check = valid_pheader(&program_header);
    if(!check) {	
#ifdef DEBUG /*This is happening*/
      kprintf("[Boot] Error loading header %d and check %d\n", i, check);
#endif
      continue;
    }
    seekto += sizeof(struct Elf64_Phdr);
    load_segment(&program_header, file);
  }

  /* The next part */
  *entry = file_header.e_entry;
  return 0;
}

/* Convert ELF flags to read-write-execute bits used throughout the kernel. */
static unsigned int elf_permission_convert(Elf64_Word pflags) {
  unsigned int permbits = 0;

  if(pflags & PF_R)
    permbits |= PERM_READ;
  if(pflags & PF_W)
    permbits |= PERM_WRITE;
  if(pflags & PF_X)
    permbits |= PERM_EXEC;

  return permbits;
}
