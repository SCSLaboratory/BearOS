#pragma once
#ifndef _IN_ASM
#include <stdint.h>

/****************************** PORT IO IN ***********************************/
/* data = inx(port); */
inline uint8_t  inb(uint16_t);
inline uint16_t inw(uint16_t);
inline uint32_t inl(uint16_t);

/* To be added by Steve Kuhn */
//inline void     insb(uint16_t,void*,int);
inline void     insw(uint16_t,void*,int);

/***************************** PORT IO OUT ************************************/
/* outx(port, data); */
inline void outb(uint16_t,uint8_t);
inline void outw(uint16_t,uint16_t);
inline void outl(uint16_t,uint32_t);

/* To be added by Steve Kuhn */
//inline void outsb(uint16_t,const void*,int);
//inline void outsw(uint16_t,const void*,int);

#endif
