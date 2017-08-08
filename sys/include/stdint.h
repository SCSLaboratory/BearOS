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

/* The unsigned series */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

/* The signed series */
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;

/* Hack to make this work with Clang's builtins when compiling tests. */
#ifndef _SIZE_T
#define _SIZE_T
typedef int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef long ssize_t;
#endif

#ifndef FS_TYPEDEF
#define FS_TYPEDEF

/*///////////////////Needed by the file system/////////////////////////*/
/* These types must be 16-bit, 32-bit or larger integer */
typedef int		INT;
typedef unsigned int	UINT;
typedef unsigned int	DWORD;

/* These types must be 8-bit integer */
typedef char		CHAR;
typedef unsigned char	UCHAR;
typedef unsigned char	BYTE;

/* These types must be 16-bit integer */
typedef short		SHORT;
typedef unsigned short	USHORT;
typedef unsigned short	WORD;
typedef unsigned short	WCHAR;

/* These types must be 32-bit integer */
typedef long LONG;
typedef unsigned long ULONG;
/*typedef unsigned long	DWORD;*/
/*///////////////////////end needed by file system////////////////////*/
#endif

/*Needed by SMP*/
typedef unsigned long uintptr_t;     
typedef long intptr_t;

/***************** NEEDED BY IFNET *******************/
typedef unsigned char u_char;
typedef unsigned int  u_int;
typedef unsigned long u_long;

typedef uint64_t off_t;
