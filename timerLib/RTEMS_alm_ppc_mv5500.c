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
#include <limits.h>             /* LONG_MAX                                     */
/* EPICS includes */
#include <epicsInterrupt.h>     /* epicsInterruptLock(), epicsInterruptUnlock() */
#include <epicsTime.h>          /* epicsTimeGetCurrent()                        */
#include <epicsTypes.h>         /* epicsUInt64                                  */
/* RTEMS includes */
#include <bsp/irq.h>            /* rtems_interrupt_catch()                      */

#include "alm.h"                /* alm_func_tbl_ts                              */


/*+**************************************************************************
 *              Local Definitions
 **************************************************************************-*/

#ifndef epicsUInt64
typedef unsigned long long epicsUInt64;
#endif

unsigned long MV64260_READ32(unsigned long base, unsigned long reg)
{
    return *((unsigned long *) (base + reg));
}

void MV64260_WRITE32_PUSH(unsigned long base, unsigned long reg, unsigned long val)
{
    *((unsigned long *) (reg + base)) = val;
    MV64260_READ32(reg, base);
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

extern unsigned int BSP_bus_frequency; /* make variable visible */

#define MV64260_REG_BASE        0xf1000000 /* Base-Addr of Marvel MV64260 Discovery Chip on the mvme5500 board*/
#define TMR_CNTR_CTRL_0_3       0x864
#define TMR_CNTR3               0x85C
#define TMR_CNTR_INT_MASK_0_3   0x86C
#define TMR_CNTR_INT_CAUSE_0_3  0x868
#define TMR_CNTR3_INT_VEC       9     

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

static unsigned long ALM_usec_to_timerticks(unsigned long delay)
{
    epicsUInt64 ticks;
    
/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    ticks = (epicsUInt64) delay * (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
}

static unsigned long ALM_timerticks_to_usec(unsigned long ticks)
{
/* if the bus frequency is lower than 4Mhz a greater datatype 
 * would be necessary, MV500 has 66Mhz -> so we can ignore this case
 */
    return (epicsUInt64) ticks / (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
}

static unsigned long ALM_timebase_to_usec(epicsUInt64 t)
{
    epicsUInt64 delay;

/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    delay = t / (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
    
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
    register unsigned long zero = 0;

    __asm__ __volatile__(                                    
                         "mttbl %0\n"
	                 "mttbu %0\n"
	                 :
	                 : "r"(zero)
    );

    /*
     * Set the timer basecount register to it's maximum value.
     * and enable or disable counting depending on the input
     * argument <enable>.
     */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3MOD_MASK); 
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR3, LONG_MAX);

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

static void ALM_int_no_op(const rtems_irq_connect_data* irq)
{
    return;
} 

static int ALM_int_IsOn(const rtems_irq_connect_data* irq)
{
    return 0;
} 

static int ALM_install_int_routine(VOID_FUNC_PTR int_handler)
{
    rtems_irq_connect_data irqInfo; 
   
    memset(&irqInfo, 0, sizeof(irqInfo));
    irqInfo.name  = TMR_CNTR3_INT_VEC;
    irqInfo.hdl  = (rtems_irq_hdl) int_handler;
    irqInfo.on   = ALM_int_no_op;
    irqInfo.off  = ALM_int_no_op;
    irqInfo.isOn = ALM_int_IsOn; 

    return BSP_install_rtems_irq_handler( &irqInfo ); 
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
    return TMR_CNTR3_INT_VEC;
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
    printf("*TMR_CNTR3              = *0x%08x = 0x%08lx\n", TMR_CNTR3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR3));
    printf("*TMR_CNTR_CTRL_0_3      = *0x%08x = 0x%08lx\n", TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3));
    printf("*TMR_CNTR_INT_CAUSE_0_3 = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_CAUSE_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3));
    printf("*TMR_CNTR_INT_MASK_0_3  = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3));
}
