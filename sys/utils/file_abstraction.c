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

#include <ff_const.h>
#include <disklabel.h>
#include <kstdio.h>
#include <kmalloc.h>
#include <memory.h>
#include <fatfs.h>
#include <file_abstraction.h>
#include <kstring.h>

/* -----------------------------------------------------------------------------
 * -------------- Bear Project Filesystem Abstraction Stuff --------------------
 * ---------------------------------------------------------------------------*/

FATFS fs; /* This is used by Fat FS code */
static size_t file_offset;

struct FAT_ctx {
  FIL fd;
  unsigned int status;
  int return_code;
};

void fs_error(FRESULT code) {
  kprintf("Filesystem: failed with code %d\n", code);
  asm volatile("cli \n\t"
	       "hlt");
}

/****************************** PUBLIC FUNCTIONS *****************************/

/* Set up the filesystem for use. */
void FAT_init() {
  struct partition *part;
  uint64_t fs_offset;
  FRESULT rc;

  fs_offset = *((uint16_t *)SLICE_OFFSET);
  fs_offset *= 512;
  /* Read from the first partition. Might want to change this later to give
   * the user a choice. */
  part = (struct partition *)(SLICE_MBR_START + sizeof(struct disklabel));
  fs_offset += part->boffset;

  rc = f_mount_offset(0, &fs, fs_offset);
  if (rc) fs_error(rc);
}


void *FAT_open(char *filename) {
  struct FAT_ctx *ctx = kmalloc_track(FILE_ABSTRACTION_SITE,sizeof(struct FAT_ctx));

  ctx->return_code = f_open(&(ctx->fd), filename, FA_READ);
  return ctx;
}

void FAT_close(void *arg) {
  struct FAT_ctx *ctx = (struct FAT_ctx *)arg;

  ctx->return_code = f_close(&(ctx->fd));

  kfree_track(FILE_ABSTRACTION_SITE,ctx);
}

void FAT_read(void *arg, void *dest, size_t size) {
  struct FAT_ctx *ctx = (struct FAT_ctx *)arg;
  //printf("Elf FAT Base/memory/dst is %x, size is %x\n", dest, size  );
  ctx->return_code = f_read(&(ctx->fd), dest, size, &(ctx->status));
	 
}

void FAT_seek(void *arg, size_t pos) {
  struct FAT_ctx *ctx = (struct FAT_ctx *)arg;

  ctx->return_code = f_lseek(&(ctx->fd), pos);
}

int FAT_error_check(void *arg) {
  struct FAT_ctx *ctx = (struct FAT_ctx *)arg;

  return ctx->return_code;
}

/* The following functions implement memory IO for the ELF loader
 * This becomes necessary since we are loading ELF binaries over the 
 * network into memory.
 */
void *mem_open() {
  struct FAT_ctx *ctx = kmalloc_track(FILE_ABSTRACTION_SITE,sizeof(struct FAT_ctx));

  ctx->return_code = FR_OK;
  return ctx;
}

void mem_close(void *arg) {
  struct FAT_ctx *ctx = (struct FAT_ctx *)arg;
  
  kfree_track(FILE_ABSTRACTION_SITE,ctx);
}

void mem_read(void *arg, void *dest, size_t size) {
  /* We can safely ignore *arg since there are no FDs in 
   * memory land 
   */
  kmemcpy(dest,(void*)(*(uint64_t*)(BINARY_LOCATION)) + (uint64_t)file_offset, size); 
}

void mem_seek(void *arg, size_t pos) {
  /* Seek will set the offset into the file
   * Internally that will be an offset to BINARY_LOCATION
   */
  file_offset = pos;
}

int mem_error_check(void *arg) {
  /* Dummy function, needed by ELF loader
   * Since memory I/O will fall on its face
   * anyway if we are using it incorrectly, we 
   * will not perform any check here 
   */
  return FR_OK;
}
