/*+*********************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * Description: PowerPC decrementer timer #3 used to set interrupt
 *              user after a specified number of microseconds.
 *
 *              
 *
 * Author(s):   Ben Franksen
 *              Dan Eichel
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
 * Copyright (c) 2008, 2009 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/

#include <limits.h>             /* ULONG_MAX                                    */
/* epics */
#include <epicsInterrupt.h>     /* epicsInterruptLock(), epicsInterruptUnlock() */
#include <epicsTime.h>          /* epicsTimeGetCurrent()                        */
#include <epicsTypes.h>         /* epicsUInt64                                  */
/* vxworks */
#include <intLib.h>             /* intConnect(), intEnable()                    */
#include <vxLib.h>              /* vxTimeBaseGet(), vxTimeBaseSet()             */ 
#include <sysLib.h>             /* sysOutLong, sysInLong                        */
#include <mv5500/mv64260.h>     /* MV64260_WRITE32_PUSH(), MV64260_READ32()     */
#include <mv5500/mv5500A.h>     /* register offsets to decrementer chip         */
#include <arch/ppc/ivPpc.h>     /* INUM_TO_IVEC                                 */

#include "alm.h"


/*+**************************************************************************
 *              Local Definitions
 **************************************************************************-*/

#ifndef epicsUInt64
typedef unsigned long long epicsUInt64;
#endif

#define TMR_CNTR3_INT_VEC (INUM_TO_IVEC(ICI_MICL_INT_NUM_9))
#define TMR_CNTR3_INT_LVL (ICI_MICL_INT_NUM_9) /* timer interrupt level */

#define ULONGLONG_MAX 0xffffffffffffffffull
#define USECS_PER_SEC 1000000ull 


/*+**************************************************************************
 *              Parameter Definitions
 **************************************************************************-*/

#define TMR_CNTR_CTRL_TC3EN_MASK        (1 << 24)
#define TMR_CNTR_CTRL_TC3MOD_MASK       (1 << 25)
#define TMR_CNTR_INT_CAUSE_TC3_MASK     (1 << 3)
#define TMR_CNTR_INT_MASK_TC3_MASK      (1 << 3)


/*+**************************************************************************
 *
 * Conversion routines
 *
 * These routines convert between timebase resp. timer ticks and microseconds.
 *
 **************************************************************************-*/

/* On MV5500 boards the decrementer & timer use the same 
 * frequency: both were triggered with a quarter of the bus cycle
 */

UINT32 sysCpuBusSpd(void); /* defined in sysMv64260Smc.c, return cpu bus speed (in Hz)  */

static unsigned long ALM_usec_to_timerticks(unsigned long delay)
{
    epicsUInt64 ticks;
    
/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    ticks = (epicsUInt64) delay * (((epicsUInt64) sysCpuBusSpd() / 4ULL) / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
}

static unsigned long ALM_timerticks_to_usec(unsigned long ticks)
{
/* if the bus frequency is lower than 4Mhz a greater datatype 
 * would be necessary, MV500 has 66Mhz -> so we can ignore this case
 */
    return (epicsUInt64) ticks / (((epicsUInt64) sysCpuBusSpd() / 4ULL) / USECS_PER_SEC);
}

static unsigned long ALM_timebase_to_usec(epicsUInt64 ticks)
{
    epicsUInt64 delay;

/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    delay = ticks / (((epicsUInt64) sysCpuBusSpd() / 4ULL) / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
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
    UINT32 tbu = 0, tbl = 0;

    vxTimeBaseGet(&tbu, &tbl);
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
    UINT32 tbu = 0, tbl = 0;

    vxTimeBaseGet(&tbu,&tbl); 
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
        
    /* disable counter 3 */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);

    /* set counter mode */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3MOD_MASK); 
    
    /* write value to count from */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR3, delay_in_timerticks);

    /* start counter 3 */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) | TMR_CNTR_CTRL_TC3EN_MASK);
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
    /*
     * Set the timer basecount register to it's maximum value.
     * and enable or disable counting depending on the input
     * argument <enable>.
     */
    vxTimeBaseSet(0, 0); 
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3MOD_MASK); 
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR3, ULONG_MAX);

    if (!enable)
    { 
        MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);
    }
    else
    {        
        MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) | TMR_CNTR_CTRL_TC3EN_MASK);
    }
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
    
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3) & ~TMR_CNTR_INT_MASK_TC3_MASK); 
	
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
    
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3) | TMR_CNTR_INT_MASK_TC3_MASK); 

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
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3) & ~TMR_CNTR_INT_CAUSE_TC3_MASK);  
}

/*+**************************************************************************
 * 
 * Function:    ALM_install_int_routine
 *
 * Description: This routine connects the interupt vector table offset
 *              used for timer #3 with the routine to be called.
 *
 * Arg(s) In:   Interrupt handler.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   OK or ERROR.
 *
 **************************************************************************-*/ 

static int ALM_install_int_routine(VOID_FUNC_PTR int_handler)
{
   int errorcode;
   if (!(errorcode = intConnect(TMR_CNTR3_INT_VEC, int_handler, 0)))
       errorcode = intEnable(TMR_CNTR3_INT_LVL);
   
   return errorcode;
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
    return TMR_CNTR3_INT_LVL;
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
/* do nothing, because the mv64260 does not support changing the interrupt level. Function are left for compatability. */
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

alm_func_tbl_ts alm_mv5500Tbl_s = {
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

alm_func_tbl_ts *alm_ppc_mv5500_tbl_ps = &alm_mv5500Tbl_s;


/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

#include <stdio.h>         /* printf */

void ALM_dump_registers(void)
{
    printf("*TMR_CNTR3              = *0x%08x = 0x%08lx\n", TMR_CNTR3, sysInLong(TMR_CNTR3));
    printf("*TMR_CNTR_CTRL_0_3      = *0x%08x = 0x%08lx\n", TMR_CNTR_CTRL_0_3, sysInLong(TMR_CNTR_CTRL_0_3));
    printf("*TMR_CNTR_INT_CAUSE_0_3 = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_CAUSE_0_3, sysInLong(TMR_CNTR_INT_CAUSE_0_3));
    printf("*TMR_CNTR_INT_MASK_0_3  = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_MASK_0_3, sysInLong(TMR_CNTR_INT_MASK_0_3));
}
