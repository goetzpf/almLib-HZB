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

#include <limits.h>             /* ULONG_MAX                                    */
#include <devLib.h>             /* devConnectInterruptVME                       */ 
#include <epicsInterrupt.h>     /* epicsInterruptLock & epicsInterruptUnlock    */

#include "timer.h"

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

void timer_init(void)
{
}

unsigned long timer_get_stamp(void)
{
    /* Read the tick timer counter register */
    return *MCC_TIMER4_CNT;
}

double timer_get_stamp_double(void)
{
    /* Read the tick timer counter register */
    return (double) *MCC_TIMER4_CNT / (double) USECS_PER_SEC;
}

void timer_setup(unsigned long delay)
{
    int lock_stat = epicsInterruptLock();
    /* Load tick timer compare register */
    *MCC_TIMER4_CMP = *MCC_TIMER4_CNT + max(delay,int_inhibit);
    /* Enable counting, disable clear-on-compare, and clear overflow counter */
    *MCC_TIMER4_CR = TIMER4_CR_CEN | TIMER4_CR_COVF;
    epicsInterruptUnlock(lock_stat);
}

void timer_reset(int enable)
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

void timer_disable(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR &= ~T4_IRQ_CR_IEN;   /* clear irq enable bit */
    epicsInterruptUnlock(lock_stat);
}

void timer_enable(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR |= T4_IRQ_CR_IEN;    /* set irq enable bit */
    epicsInterruptUnlock(lock_stat);
}

void timer_int_ack(void)
{
    int lock_stat = epicsInterruptLock();
    *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR;   /* Clear IRQ's */
    epicsInterruptUnlock(lock_stat);
}

int  timer_install_int_routine(VOID_FUNC_PTR int_handler)
{
    return devConnectInterruptVME(TIMER4_INT_VEC, int_handler, 0);
}

unsigned timer_get_int_level(void)
{
    /*
     * Read the tick timer interrupt control register
     * and extract the current IRQ priority level.
     */
    return (unsigned) (*MCC_T4_IRQ_CR & T4_IRQ_CR_IL);
}

void timer_set_int_level(unsigned level)
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

unsigned long timer_get_max_delay(void)
{
    return ULONG_MAX;
}
