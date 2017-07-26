extern void *OpenPIC;
#define BASE           (unsigned long) OpenPIC

#define TIMER3_INT_VEC 0x3

#include <bsp/irq.h>
#include "alm_ppcDec.c"
