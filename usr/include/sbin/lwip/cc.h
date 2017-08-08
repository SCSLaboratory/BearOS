/*
 *	This file translates bear types into LwIP types 
 *
 *	Author: Martin Osterloh
 */


#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <sbin/lwip/opt.h>

#ifdef USER
#include <sys/types.h>
#endif

/*
 *	TYPEDEFINITIONS 
 */

//#define sys_sem_valid(sem) (((sem) != NULL) && (*(sem) != NULL))
//#define sys_sem_set_invalid(sem) do { if((sem) != NULL) { *(sem) = NULL; }} while(0)
/* sem is a pointer to an integer */
#define sys_sem_valid(sem) (((sem) != NULL) && (*(sem) != 0))
#define sys_sem_set_invalid(sem) do { if((sem) != NULL) { *(sem) = 0; }} while(0)

/* let sys.h use binary semaphores for mutexes */
#define LWIP_COMPAT_MUTEX 1

#define MBOX_SIZE 100
//#define sys_mbox_valid(mbox) (((mbox) != NULL) && (*(mbox) != NULL))
//n#define sys_mbox_set_invalid(mbox) do { if((mbox) != NULL) { *(mbox) = NULL; }} while(0)
/* mbox is a point to an integer */
#define sys_mbox_valid(mbox) (((mbox) != NULL) && (*(mbox) != 0))
#define sys_mbox_set_invalid(mbox) do { if((mbox) != NULL) { *(mbox) = 0; }} while(0)

/* Define platform endianness */
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif /* BYTE_ORDER */

/* Define generic types used in lwIP */
typedef unsigned   char    u8_t;
typedef signed     char    s8_t;
typedef unsigned   short   u16_t;
typedef signed     short   s16_t;
typedef unsigned   int     u32_t;
typedef signed     int     s32_t;

typedef unsigned long mem_ptr_t;

/* Define (sn)printf formatters for these lwIP types */
#define X8_F  "X"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

/* If only we could use C99 and get %zu */
#if defined(__x86_64__)
#define SZT_F "u"
#else
#define SZT_F "u"
#endif

/* Compiler hints for packing structures */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Plaform specific diagnostic output */

#define LWIP_PLATFORM_DIAG(x)	do {printf x ; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do {/*printf("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); fflush(NULL); abort(); */} while(0)

/* Use LwIP's error codes throughout the stack */
#define LWIP_PROVIDE_ERRNO /* ERROR codes are defined in arch.h */

/* Do we have a RNG? */
#define LWIP_RAND() ((u32_t)rand())

#endif /* __ARCH_CC_H__ */
