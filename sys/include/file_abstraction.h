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
/* file_abstraction.h -- provide a filesystem-agnostic way for the kernel to
 *                       read from and write to files.
 */
/* This is pretty hackish right now, because we only have one filesystem and
 * no modules. So we just define the pointers as pointing to those functions */


/* ----------------------------------------------------------------------------
 * ------------- Bear Project Filesystem Abstraction Stuff --------------------
 * --------------------------------------------------------------------------*/

/* These functions are for our pseudo-object-orientation that allow us to speak
 * abstractly about the act of reading/writing/seeking in files, by passing
 * around function pointers to generic functions (i.e., these). Whenever you
 * have a function that needs to do something as a file, it should take a
 * function pointer that corresponds to one of these prototypes, and then call
 * the pointer. This way, you aren't binding yourself to a specific filesystem
 * and it can be changed later.
 *
 * The function arguments are as follows:
 * FAT_open -- Arg 1: A filename string.
 *             Returns: FAT_ctx structure to be passed to other FAT functions.
 * FAT_close -- Arg 1: A pointer to a FAT_ctx structure.
 * FAT_read -- Arg 1: A pointer to a FAT_ctx structure.
 *             Arg 2: A pointer to the memory that FAT_read will write to.
 *             Arg 3: Size of the read.
 * FAT_seek -- Arg 1: A pointer to a FAT_ctx structure.
 *             Arg 2: Where in the file to seek to.
 * FAT_error_check -- Arg 1: A pointer to a FAT_ctx structure.
 *                    Returns: The return code of any previous functions.
 *
 * None of these functions are available in the bootloader, which does not have
 * the necessary machinery for running this kind of abstraction.
*/

/* Forward declare the functions. */
void FAT_init();
void *FAT_open(char *);
void FAT_close(void *);
void FAT_read(void *, void *, size_t);
void FAT_seek(void *, size_t);
int FAT_error_check(void *);

void *mem_open();
void mem_close(void*);
void mem_read(void*, void*, size_t);
void mem_seek(void*, size_t);
int mem_error_check(void*);

#define file_init FAT_init
#define file_open FAT_open
#define file_close FAT_close
#define file_read FAT_read
#define file_seek FAT_seek
#define file_error_check FAT_error_check
