/*+**************************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	RTEMS_alm_mcc.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Motorola MCCchip
 *              On the MOTOROLA MVME 162/167: Uses the timer 4 on the
 *              mvme16x's MCC chip. (See also timestamp timer code in
 *              /opt/vxworks/tornado202/target/src/drv/timer/mccTimer.c).
 *
 * Author(s):	Ralph Lange / Christi Luchini / Ben Franksen / Dan Eichel
 *
 * This software is copyrighted by the BERLINER SPEICHERRING-
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY.
 * The following terms apply to all files associated with the software.
 *
 * BESSY hereby grants permission to use, copy and modify this software
 * and its documentation for non-commercial, educational or research
 * purposes, provided that existing copyright notices are retained in
 * all copies.
 *
 * The receiver of the software provides BESSY with all enhancements,
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE
 * OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN
 * IF BESSY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NON-INFRINGEMENT. THIS SOFTWARE IS PROVIDED
 * ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS OR MODIFICATIONS.
 *
 * Copyright (c) 1996, 1997, 1999, 2006, 2009
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/

#include <limits.h>             /* ULONG_MAX                                    */
#include <devLib.h>             /* devConnectInterruptVME                       */ 
#include <epicsInterrupt.h>     /* epicsInterruptLock & epicsInterruptUnlock    */

#include "alm.h"

/*+**************************************************************************
 *	       Parameter definitions
 **************************************************************************-*/
/* define the constants/macros manually because RTEMS usually does'nt use/define them! */
/* under vxWorks these constants are defined in mcchip.h, mv162.h & vmechip2.h */  

#define TIMER_INT_LEVEL    2    /* Default timer 4 interrupt level */
#define TIMER4_INT_VEC     0x43 /* vector offset number -> MCC_INT_VEC_BASE=0x40 + MCC_INT_TT4=0x03 */
#define TIMER4_CR_CEN      0x01 /* Counter enable */
#define TIMER4_CR_COVF     0x04 /* Clear overflow counter */
#define T4_IRQ_CR_IL       0x07 /* Interrupt level bits of timer 4 interrupt control register */
#define T4_IRQ_CR_ICLR     0x08 /* Clear IRQ */
#define T4_IRQ_CR_IEN      0x10 /* Interrupt 4 Enable */

#define MCC_BASE_ADRS      0xfff42000
#define MCC_ADRS(reg)      ((unsigned char *) (MCC_BASE_ADRS + reg))
#define	MCC_ADRS_INT(reg)  ((unsigned int *) (MCC_BASE_ADRS + reg)) 

/* Tick Timer 4 Count Reg */
#define MCC_TIMER4_CNT     MCC_ADRS_INT(0x3C)
/* Tick Timer 4 Cmp Reg */
#define MCC_TIMER4_CMP     MCC_ADRS_INT(0x38) 
/* Tick Timer 4 Ctrl Reg */
#define MCC_TIMER4_CR      MCC_ADRS(0x1E)
/* Tick Timer 4 Inter CR */
#define MCC_T4_IRQ_CR      MCC_ADRS(0x18) 

 /*+**************************************************************************
 *	       Local variables
 **************************************************************************-*/
static unsigned long int_inhibit = 10;   /* = minimum delay */

/*+**************************************************************************
 *
 * Function:	ALM_get_stamp
 *
 * Description:	Returns a time stamp from the alarm timer (32 bit;
 *              1 us resolution). Please note that this timer is
 *              read and writable at anytime.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Time stamp value.
 *
 **************************************************************************-*/

static unsigned long ALM_get_stamp(void)
{
    /* Read the tick timer counter register */
    return *MCC_TIMER4_CNT;
}

/*+**************************************************************************
 *
 * Function:	ALM_get_stamp_double
 *
 * Description:	Returns a time stamp from the alarm timer (32 bit;
 *              1 us resolution). Please note that this timer is
 *              read and writable at anytime.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Time stamp value.
 *
 **************************************************************************-*/

static double ALM_get_stamp_double(void)
{
    /* Read the tick timer counter register */
    return (double) *MCC_TIMER4_CNT / (double) USECS_PER_SEC;
}

/*+**************************************************************************
 *
 * Function:    ALM_setup
 *
 * Description:	Set up the counter to go off in <delay> microseconds.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	delay  -  Demanded timer delay in microseconds.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

static void ALM_setup(unsigned long delay)
{
    int lock_stat = epicsInterruptLock();
    /* Load tick timer compare register */
    *MCC_TIMER4_CMP = *MCC_TIMER4_CNT + max(delay,int_inhibit);
    /* Enable counting, disable clear-on-compare, and clear overflow counter */
    *MCC_TIMER4_CR = TIMER4_CR_CEN | TIMER4_CR_COVF;
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:	ALM_reset
 *
 * Description:	Sets the timer to its min value and dis- or enable counting.
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
    int lock_stat = epicsInterruptLock();
    *MCC_TIMER4_CNT = 0;
    /* Enable counting, disable clear-on-compare, and clear overflow counter */
    *MCC_TIMER4_CR = TIMER4_CR_CEN | TIMER4_CR_COVF;
    if (enable) {
        *MCC_T4_IRQ_CR |= T4_IRQ_CR_IEN;    /* set irq enable bit */
    } else {
        *MCC_T4_IRQ_CR &= ~T4_IRQ_CR_IEN;   /* clear irq enable bit */
    }
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:	ALM_disable
 *
 * Description:	Disable the counter interrupts.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

static void ALM_disable(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR &= ~T4_IRQ_CR_IEN;   /* clear irq enable bit */
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:	ALM_enable
 *
 * Description:	Enable the timer.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

static void ALM_enable(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR |= T4_IRQ_CR_IEN;    /* set irq enable bit */
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:	ALM_int_ack
 *
 * Description:	Acknowledge the interrupt.
 *
 * Arg(s) In:	None
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

static void ALM_int_ack(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR;   /* Clear IRQ's */
    epicsInterruptUnlock(lock_stat);
}

/*+**************************************************************************
 *
 * Function:    ALM_install_int_routine
 *
 * Description: This routine installs the interupt handler
 *
 * Arg(s) In:   Interrupt handler.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   OK or ERROR.
 *
 **************************************************************************-*/
static int  ALM_install_int_routine(VOID_FUNC_PTR int_handler)
{
    return devConnectInterruptVME(TIMER4_INT_VEC, int_handler, 0);
}

/*+**************************************************************************
 *
 * Function:    ALM_get_int_level
 *
 * Description: Return the currently set interrupt vector priority level.
 *              An interrupt level of 0 indicates that
 *              interrupts are disabled.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector priority level (0-7).
 *
 **************************************************************************-*/

static unsigned ALM_get_int_level(void)
{
    /*
     * Read the tick timer interrupt control register
     * and extract the current IRQ priority level.
     */
    return (unsigned) (*MCC_T4_IRQ_CR & T4_IRQ_CR_IL);
}

/*+**************************************************************************
 *
 * Function:    ALM_set_int_level
 *
 * Description: Set interrupt priority level.
 *              An interrupt level of 0 disables interrupts.
 *
 *              Note: Only the lowest 3 bits of the argument are used.
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
    /* Must lock interrupt because this is a read-modify-write cycle */
    int lock_stat = epicsInterruptLock();
    if (!level) {
        /* use default int level */
        level = TIMER_INT_LEVEL;
    }
    /* Set irq level bits of the tick timer interrupt control register */
    *MCC_T4_IRQ_CR = (*MCC_T4_IRQ_CR & ~T4_IRQ_CR_IL) | (level & 0x07);
    epicsInterruptUnlock(lock_stat);
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
    return ULONG_MAX;
}


/*+**************************************************************************
 *
 * Timer function entry table
 *
 **************************************************************************-*/

static alm_func_tbl_ts alm_mccTbl_s = {
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
