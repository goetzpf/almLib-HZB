/*==========================================================
                             alarm
  ==========================================================

Copyright 2022 Helmholtz-Zentrum Berlin für Materialien und Energie GmbH
<https://www.helmholtz-berlin.de>

This file is part of the alarm EPICS support module.

alarm is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

alarm is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with alarm.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <limits.h>         /* std: LONG_MAX                                       */

#include <epicsInterrupt.h> /* EPICS: epicsInterruptLock(), epicsInterruptUnlock() */
#include <epicsTypes.h>     /* EPICS: epicsUInt64                                  */
#include <epicsVersion.h>

#include "timer.h"          /* alm_func_tbl_ts                                     */

#include "ppc_timebase_reg.c"

/*+**************************************************************************
 *              Local Definitions
 **************************************************************************-*/

#if EPICS_VERSION==3 && EPICS_REVISION<=14
typedef unsigned long long epicsUInt64;
#endif

#ifndef ULONGLONG_MAX
#define ULONGLONG_MAX 0xffffffffffffffffull
#endif

#define USECS_PER_SEC 1000000ull 

/* offsets of timer registers, all supported boards use the same decrementer chip,  */
/* only the start addr differs depending of board version and installed Mpic or Epic chip */
#define TIMER3_BASE_CT_REG 0x11d0 /* base count register                */
#define TIMER3_VEC_PRI_REG 0x11e0 /* interrupt vector priority register */
#define TIMER3_DEST_REG    0x11f0 /* destination register               */

/*+**************************************************************************
 *              Parameter Definitions
 **************************************************************************-*/

#define DESTINATION_CPU0   0x00000001
#define VECTOR_MASK        0x000000ff
#define PRIORITY_MASK      0x000f0000
#define TIMER_DISABLE      0x80000000
#define TIMER_COUNT_MASK   0x7FFFFFFF
#define TIMER_INT_LEVEL    2           /* Default timer interrupt level */

#define DEC_CLK_FREQ       16666666ULL /* Bus cycles (66Mhz) / 4        */
#define TMR_CLK_FREQ        8333333ULL /* Bus cycles (66Mhz) / 8        */


/*
PCI bus I/O operations must adhere to little endian byte ordering.  Thus if
an I/O operation larger than one byte is performed, the lower I/O addresses
contain the least signficant bytes of the multi-byte quantity of interest.

For architectures that adhere to big-endian byte ordering, byte-swapping
must be performed.  The architecture-specific byte-order translation is done
as part of the I/O operation in the following routine.
*/
static void sysPciOutLong(volatile unsigned long adrs, unsigned long val)
{
    const unsigned long off = BASE;
    
    __asm__ __volatile__("stwbrx %1, %0, %2" :: "b"(off), "r"(val), "r"(adrs));
}

static unsigned long sysPciInLong(volatile unsigned long adrs)
{
    const unsigned long off = BASE;
    register unsigned long rval;
    
    __asm__ __volatile__("lwbrx %0, %1, %2" : "=r"(rval) : "b"(off), "r"(adrs));
    return rval;
} 

/*+**************************************************************************
 *
 * Conversion routines
 *
 * These routines convert between timebase resp. timer ticks and microseconds.
 *
 **************************************************************************-*/

static unsigned long timer_usec_to_timerticks(unsigned long delay)
{
    epicsUInt64 ticks;
    
    ticks = (epicsUInt64) delay * (TMR_CLK_FREQ / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
}

static unsigned long timer_timerticks_to_usec(unsigned long ticks)
{
    return (epicsUInt64) ticks / (TMR_CLK_FREQ / USECS_PER_SEC);
}

static unsigned long timer_timebase_to_usec(epicsUInt64 t)
{
    epicsUInt64 delay;

    delay = t / (DEC_CLK_FREQ / USECS_PER_SEC);
    
    return (unsigned long) (delay & 0xFFFFFFFF);
} 

/*+**************************************************************************
 *
 * API
 *
 *
 **************************************************************************-*/

void timer_init(void)
{
}

unsigned long timer_get_stamp(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return timer_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl);
}

double timer_get_stamp_double(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return (double) timer_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl) \
        / (double) USECS_PER_SEC;
}

void timer_setup(unsigned long delay)
{
    unsigned long delay_in_timerticks;

    delay_in_timerticks = timer_usec_to_timerticks(delay);

    /*
     * Stop counter by setting bit 31 in the base count register.
     * This is necessary because the counter register gets reloaded
     * only if this bit toggles from high to low.
     */
    sysPciOutLong(TIMER3_BASE_CT_REG, TIMER_DISABLE);

    /*
     * Load timer count register with the new value.
     * This also starts counting, see explanation above.
     */
    sysPciOutLong(TIMER3_BASE_CT_REG, delay_in_timerticks & TIMER_COUNT_MASK);
}

void timer_reset(int enable)
{
    unsigned long value = TIMER_COUNT_MASK;
    register unsigned long zero = 0;

    __asm__ __volatile__(                                    
                         "mttbl %0\n"
	                 "mttbu %0\n"
	                 :
	                 : "r"(zero)
    );
    if (!enable)
        value |= TIMER_DISABLE;
    sysPciOutLong(TIMER3_BASE_CT_REG, value);
    sysPciOutLong(TIMER3_DEST_REG, DESTINATION_CPU0);
}

void timer_disable(void)
{
    int lock_stat = epicsInterruptLock();
    sysPciOutLong(TIMER3_VEC_PRI_REG, sysPciInLong(TIMER3_VEC_PRI_REG) | TIMER_DISABLE);
    epicsInterruptUnlock(lock_stat);
}

void timer_enable(void)
{
    int lock_stat = epicsInterruptLock();
    sysPciOutLong(TIMER3_VEC_PRI_REG, sysPciInLong(TIMER3_VEC_PRI_REG) & ~TIMER_DISABLE);
    sysPciOutLong(TIMER3_DEST_REG, DESTINATION_CPU0);
    epicsInterruptUnlock(lock_stat);
}

void timer_int_ack(void)
{
    sysPciOutLong(TIMER3_BASE_CT_REG, TIMER_DISABLE);
}

#if defined ( __rtems__ )
static void timer_int_no_op(const rtems_irq_connect_data* irq)
{
    return;
} 

static int timer_int_IsOn(const rtems_irq_connect_data* irq)
{
    return 0;
} 
#endif

int timer_install_int_routine(VOID_FUNC_PTR int_handler)
{
    unsigned long rd_val = 0;
    unsigned vector = 0;
#if defined ( __rtems__ )
    rtems_irq_connect_data irqInfo; 
#endif

    rd_val = sysPciInLong(TIMER3_VEC_PRI_REG);
    vector = rd_val & VECTOR_MASK;
    
#if defined ( __rtems__ )
/*  Unfortunally RTEMS has'nt a generally API-Interface for interrupt usage.
    Maybe in later versions it will be introduced. In version 4.9.0 for ppc-targets
    is the common method to install an ISR the following sequence.

    supplement: EPICS V3.14.10 also has not a OSI-wrapper for interrupt handling
    ecxept handling of VME-interrupts                                               */
    
    memset(&irqInfo, 0, sizeof(irqInfo));
    irqInfo.name  = vector;
    irqInfo.hdl  = (rtems_irq_hdl) int_handler;
    irqInfo.on   = timer_int_no_op;
    irqInfo.off  = timer_int_no_op;
    irqInfo.isOn = timer_int_IsOn; 

    return !BSP_install_rtems_irq_handler( &irqInfo ); 
#else /* vxWorks */
    return intConnect((void *) vector, int_handler, 0);
#endif
}

unsigned timer_get_int_level(void)
{
    return (sysPciInLong(TIMER3_VEC_PRI_REG) & PRIORITY_MASK) >> 16;
}

void timer_set_int_level(unsigned level)
{
    int lock_key = epicsInterruptLock();
    if (!level) {
        /* use default int level */
        level = TIMER_INT_LEVEL;
    }
    sysPciOutLong(TIMER3_VEC_PRI_REG, (level << 16) | (TIMER3_INT_VEC));
    epicsInterruptUnlock(lock_key);
}

unsigned long timer_get_max_delay(void)
{
    static unsigned long alm_max_delay = 0;
    if (!alm_max_delay) {
        alm_max_delay = timer_timerticks_to_usec(LONG_MAX);
    }
    return alm_max_delay;
}

/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

#include <stdio.h>

void timer_dump_registers(void)
{
    unsigned long base = BASE;
    unsigned long *base_ct_reg = (unsigned long *) (base + TIMER3_BASE_CT_REG);
    unsigned long *vec_pri_reg = (unsigned long *) (base + TIMER3_VEC_PRI_REG);
    unsigned long *dest_reg = (unsigned long *) (base + TIMER3_DEST_REG);
    
    printf("*base_ct_reg = *0x%08lx = 0x%08lx\n", (unsigned long) base_ct_reg, sysPciInLong(TIMER3_BASE_CT_REG));
    printf("*vec_pri_reg = *0x%08lx = 0x%08lx\n", (unsigned long) vec_pri_reg, sysPciInLong(TIMER3_VEC_PRI_REG));
    printf("*dest_reg    = *0x%08lx = 0x%08lx\n", (unsigned long) dest_reg, sysPciInLong(TIMER3_DEST_REG));
}
