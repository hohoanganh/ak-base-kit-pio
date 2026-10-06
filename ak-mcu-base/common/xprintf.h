/**
 ******************************************************************************
 * @Author: GaoKong
 * @Date:   13/08/2016
 ******************************************************************************
**/
#ifndef __XPRINTF_H__
#define __XPRINTF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>
#include <stdint.h>

extern void (*xfunc_out)(uint8_t);
extern void xputc(uint8_t c);
/* Supports %c %s %d %u %x %X, width + zero pad (%08X), left align (%-8s).
 * 32-bit arguments only; no %l, no %f. */
extern void xprintf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif //__XPRINTF_H__
