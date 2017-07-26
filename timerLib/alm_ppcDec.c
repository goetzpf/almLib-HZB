/*+*********************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * Description: PowerPC decrementer timer #3 used to set interrupt
 *              user after a specified number of microseconds.
 *
 *              The software for the timer chip found on the mv162 
 *              is in the file alm_mcc.c
 *
 * Author(s):   John Moore (Argon Engineering  vxWorks news group)
 *              Ben Franksen, Dan Eichel
 *
 * This software is copyrighted by the BERLINER SPEICHERRING 
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY. 
 * The following terms apply to all files associated with the 
 * software. 
 *
 * BESSY hereby grants permission to use, copy, and modify this 
 * software and its documentation for non-commercial educational or 
 * research purposes, provided that existing copyright notices are 
 * retained in all copies. 
 *
 * The receiver of the software provides BESSY with all enhancements, 
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, 
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING 
 * OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY 
 * DERIVATIVES THEREOF, EVEN IF BESSY HAS BEEN ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR 
 * A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS 
 * PROVIDED ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO 
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR 
 * MODIFICATIONS. 
 *
 * Copyright (c) 1997, 2009 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/

#include <limits.h>         /* std: LONG_MAX                                       */

#include <epicsInterrupt.h> /* EPICS: epicsInterruptLock(), epicsInterruptUnlock() */
#include <epicsTypes.h>     /* EPICS: epicsUInt64                                  */

#if defined ( __rtems__ )
#include <bsp/irq.h>        /* rtems_interrupt_catch()                             */
#else
#include <intLib.h>         /* intConnect()                                        */
#endif

#include "alm.h"            /* alm_func_tbl_ts                                     */

/*+**************************************************************************
 *              Local Definitions
 **************************************************************************-*/

#ifndef epicsUInt64
typedef unsigned long long epicsUInt64;
#endif

#define ULONGLONG_MAX 0xffffffffffffffffull
#define USECS_PER_SEC 1000000ull 

/* offsets of timer registers, all supported boards use the same decrementer chip,  */
/* only the start addr differs depending of board version and installed Mpic or Epic chip */
#define TIMER3_BASE_CT_REG 0x11d0 /* base count register                */
#define TIMER3_VEC_PRI_REG 0x11e0 /* interrupt vector priority register */
#define TIMER3_DEST_REG    0x11f0 /* destination register               */

#if defined ( __rtems__ )
extern void *OpenPIC;
#endif

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
#if defined ( __rtems__ )
    const unsigned long off = (unsigned long) OpenPIC;
#else
    const unsigned long off = BASE;
#endif
    
    __asm__ __volatile__("stwbrx %1, %0, %2" :: "b"(off), "r"(val), "r"(adrs));
}

static unsigned long sysPciInLong(volatile unsigned long adrs)
{
#if defined ( __rtems__ )
    const unsigned long off = (unsigned long) OpenPIC;
#else
    const unsigned long off = BASE;
#endif
    register unsigned long rval;
    
    __asm__ __volatile__("lwbrx %0, %1, %2" : "=r"(rval) : "b"(off), "r"(adrs));
    return rval;
} 

static void readTimeBaseReg(unsigned long *tbu, unsigned long *tbl)
{
    register unsigned long dummy __asm__ ("r0");
    __asm__ __volatile__(
                         "loop:	mftbu %2\n"
			 "	mftb  %0\n"
			 "	mftbu %1\n" 
			 "	cmpw  %1, %2\n"
			 "	bne   loop\n"
			 : "=r"(*tbl), "=r"(*tbu), "=r"(dummy)
			 : "1"(*tbu), "2"(dummy)
    );
}

/*+**************************************************************************
 *
 * Conversion routines
 *
 * These routines convert between timebase resp. timer ticks and microseconds.
 *
 **************************************************************************-*/

static unsigned long ALM_usec_to_timerticks(unsigned long delay)
{
    epicsUInt64 ticks;
    
    ticks = (epicsUInt64) delay * (TMR_CLK_FREQ / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
}

static unsigned long ALM_timerticks_to_usec(unsigned long ticks)
{
    return (epicsUInt64) ticks / (TMR_CLK_FREQ / USECS_PER_SEC);
}

static unsigned long ALM_timebase_to_usec(epicsUInt64 t)
{
    epicsUInt64 delay;

    delay = t / (DEC_CLK_FREQ / USECS_PER_SEC);
    
    return (unsigned long) (delay & 0xFFFFFFFF);
} 


/*+**************************************************************************
 *
 * Function:	ALM_get_stamp
 *
 * Description: Return the current timestamp, converted to microsecond
 *              units, as an unsigned 32 bit integral value.
 *              Note: We have to make sure that the returned timestamp
 *              will correctly overflow after ULONG_MAX microseconds.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in microseconds
 *
 **************************************************************************-*/

static unsigned long ALM_get_stamp(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return ALM_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl);
}

/*+**************************************************************************
 *
 * Function:	ALM_get_stamp_double
 *
 * Description: This function is used for measuring longer times 
 *              that require the full 64-bit resolution. The value is 
 *              returned in seconds.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in seconds
 *
 **************************************************************************-*/

static double ALM_get_stamp_double(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return (double) ALM_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl) \
        / (double) USECS_PER_SEC;
}

/*+**************************************************************************
 *
 * Function:    ALM_setup
 *
 * Description: Set up the counter to go off in <delay> microseconds.
 *
 * Arg(s) In:   delay  -  Demanded timer delay in microseconds.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_setup(unsigned long delay)
{
    unsigned long delay_in_timerticks;

    delay_in_timerticks = ALM_usec_to_timerticks(delay);

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

/*+**************************************************************************
 *
 * Function:	ALM_reset
 *
 * Description:	Set the timer to its max value and dis- or enable counting.
 *
 * Arg(s) In:	enable  - should counting be enabled?
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_reset(int enable)
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

/*+**************************************************************************
 *
 * Function:    ALM_disable
 *
 * Description: Disable interrupts for this timer.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_disable(void)
{
    int lock_stat = epicsInterruptLock();
    sysPciOutLong(TIMER3_VEC_PRI_REG, sysPciInLong(TIMER3_VEC_PRI_REG) | TIMER_DISABLE);
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:    ALM_enable
 *
 * Description: Enable interrupts for this timer.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_enable(void)
{
    int lock_stat = epicsInterruptLock();
    sysPciOutLong(TIMER3_VEC_PRI_REG, sysPciInLong(TIMER3_VEC_PRI_REG) & ~TIMER_DISABLE);
    sysPciOutLong(TIMER3_DEST_REG, DESTINATION_CPU0);
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:    ALM_int_ack
 *
 * Description: Disable the counter, so that no more interrupts occur.
 *              If this is not called as the first action in the interrupt
 *              handler, a vxWorks workQueue overflow panic might result.
 *              This is because the ppcDec timers automatically reload
 *              themselves with the last value.
 *
 *              Note: this has nothing to do with the actual interrupt
 *              acknowledge, which is done by vxWorks.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_int_ack(void)
{
    sysPciOutLong(TIMER3_BASE_CT_REG, TIMER_DISABLE);
}


/*+**************************************************************************
 * 
 * Function:    ALM_install_int_routine
 *
 * Description: This routine installs the interupt handler. 
 *
 * Arg(s) In:   Interrupt handler.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   OK or ERROR.
 *
 **************************************************************************-*/ 

#if defined ( __rtems__ )
static void ALM_int_no_op(const rtems_irq_connect_data* irq)
{
    return;
} 

static int ALM_int_IsOn(const rtems_irq_connect_data* irq)
{
    return 0;
} 
#endif

static int ALM_install_int_routine(VOID_FUNC_PTR int_handler)
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
    irqInfo.on   = ALM_int_no_op;
    irqInfo.off  = ALM_int_no_op;
    irqInfo.isOn = ALM_int_IsOn; 

    return !BSP_install_rtems_irq_handler( &irqInfo ); 
#else /* vxWorks */
    return intConnect((void *) vector, int_handler, 0);
#endif
}

/*+**************************************************************************
 * 
 * Function:    ALM_get_int_level
 *
 * Description: Return the currently set interrupt priority level.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt priority level (0-15).
 *
 **************************************************************************-*/ 

static unsigned ALM_get_int_level(void)
{
    return (sysPciInLong(TIMER3_VEC_PRI_REG) & PRIORITY_MASK) >> 16;
}

/*+**************************************************************************
 * 
 * Function:    ALM_set_int_level
 *
 * Description: Set the interupt level for timer #3.
 *
 * Arg(s) In:   Interrupt level.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/ 

static void ALM_set_int_level(unsigned level)
{
    int lock_key = epicsInterruptLock();
    if (!level) {
        /* use default int level */
        level = TIMER_INT_LEVEL;
    }
    sysPciOutLong(TIMER3_VEC_PRI_REG, (level << 16) | (TIMER3_INT_VEC));
    epicsInterruptUnlock(lock_key);
}

/*+**************************************************************************
 *
 * Function:    ALM_get_max_delay
 *
 * Description: Return the maxmimum possible delay (in microseconds)
 *              that is accepted as argument to ALM_setup.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Maxmimum possible delay.
 *
 **************************************************************************-*/

static unsigned long ALM_get_max_delay(void)
{
    static unsigned long alm_max_delay = 0;
    if (!alm_max_delay) {
        alm_max_delay = ALM_timerticks_to_usec(LONG_MAX);
    }
    return alm_max_delay;
}


/*+**************************************************************************
 *
 * Timer function entry table
 *
 **************************************************************************-*/

static alm_func_tbl_ts alm_ppcDecTbl_s = {
    ALM_setup,
    ALM_reset,
    ALM_enable,
    ALM_disable,
    ALM_int_ack,
    ALM_install_int_routine,
    ALM_get_int_level,
    ALM_set_int_level,
    ALM_get_max_delay,
    ALM_get_stamp,
    ALM_get_stamp_double
};

/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

#include <stdio.h>

void ALM_dump_registers(void)
{
#if defined ( __rtems__ )
    unsigned long base = (unsigned long) OpenPIC;
#else
    unsigned long base = BASE;
#endif    
    unsigned long *base_ct_reg = (unsigned long *) (base + TIMER3_BASE_CT_REG);
    unsigned long *vec_pri_reg = (unsigned long *) (base + TIMER3_VEC_PRI_REG);
    unsigned long *dest_reg = (unsigned long *) (base + TIMER3_DEST_REG);
    
    printf("*base_ct_reg = *0x%08lx = 0x%08lx\n", (unsigned long) base_ct_reg, sysPciInLong(TIMER3_BASE_CT_REG));
    printf("*vec_pri_reg = *0x%08lx = 0x%08lx\n", (unsigned long) vec_pri_reg, sysPciInLong(TIMER3_VEC_PRI_REG));
    printf("*dest_reg    = *0x%08lx = 0x%08lx\n", (unsigned long) dest_reg, sysPciInLong(TIMER3_DEST_REG));
}
