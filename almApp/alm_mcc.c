static char RcsId[]      =
"@(#)$Id: alm_mcc.c,v 2.5 2004/06/22 09:27:06 luchini Exp $";

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_mcc.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Motorola MCCchip
 *              On the MOTOROLA MVME 162/167:
 *                Uses the timer 4 on the mvme16x's MCC chip. (See also 
 *		  timestamp timer code in ~vw/src/drv/timer/mccTimerTS.c)
 *
 *             The software for the decrementer timer chip (MPIC) found on
 *             most powerPc processors is located in the file alm_ppcDec.c
 *
 *             Debug messages are used in the macro ALM_DEBUG_MSG().
 *             This macro is defined to be empty if the definition
 *                   INCLUDE_ALM_DEBUG
 *             has not been defined prior to alm_debug.h. This
 *             definition can be included in the makefile during 
 *             compilation.
 
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.5 $
 * $Date: 2004/06/22 09:27:06 $
 *
 * $Author: luchini $
 *
 * Revision log at end of file
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
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/

#define INCLUDE_ALM_DEBUG      /* for ALM_DEBUG_MSG() macro w/logMsg()   */
#include <vxWorks.h>           /* for OK and ERROR                       */
#include <intLib.h>            /* for intLock(),intUnlock()              */
#include <logLib.h>            /* for logMsg()                           */
#include <alm.h>               /* for  alm_status_ts                     */
#include <alm_mcc.h>           /* for MCC_BASE_ADRS,MCC_INT_VEC_BASE,etc */
                               /* for MAGIC_P,MAGIC_25,MAGIC_32,etc      */
#include <alm_debug.h>         /* for ALM_DEBUG_MSG()                    */
#include <alm_debug.c>         /* for alm_debug_msg(),alm_set_debug_msg()*/


/*+**************************************************************************
 *	       Local variables
 **************************************************************************-*/
static unsigned long  int_inhibit=0; 
static unsigned long  int_subtract=0;

/*+**************************************************************************
 *	       Global variables - (see almLib.c)
 **************************************************************************-*/
alm_status_ts        alm_status = {FALSE,FALSE,FALSE,FALSE,0,
                                   1,TIMER4_INT_VEC,
                                   FALSE,FALSE,0};



/*+**************************************************************************
 *
 * Function:	alm_get_stamp
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

unsigned long alm_get_stamp (void)
{
   /* Read the tick timer counter register */
   return(*MCC_TIMER4_CNT);
}

/*+**************************************************************************
 *
 * Function:	alm_setup
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

void alm_setup (unsigned long delay)
{
   register int lock_key=0;

   ALM_DEBUG_MSG(5,"alm_setup: Entering function.",0,0);

   lock_key = intLock();	/* Lock interrupts             */
   alm_status.running = TRUE;	/* Set running flag            */

   /* The timer has a resolutin of 1microsecond. When free running, 
    * it will roll over every 71.6 minutes.  
    * 
    * Each timer has a 
    *   32-bit counter register
    *   32-bit compare register
    *    4-bit overflow register
    *    enable bit
    *    overflow clear bit, 
    *    clear-on-compare enable bit
    * The counter is readable and writeable at any time. 
    * When enabled in free-run mode, it increments every 1usec.
    * When the counter is enabled in clear-on-compare mode, it increments
    * every 1usec until the counter value matches the value in the compare
    * register.  When a match occurs, the counter is cleared. When a match
    * occurs in either mode, an interrupt is sent to the local bus interrupter
    * and the overflow counter is incremented. An interrupt to the local bus 
    * is only generated if the tick timer interrupt is enabled by the local
    * bus interrupter. The overflow counter can be cleared by writing a 1
    * to the overflow clear bit.
    */

    /* load compare register with the number of micro seconds */
   *MCC_TIMER4_CMP = *MCC_TIMER4_CNT +
                      (delay < int_inhibit ? int_inhibit : delay);

   /* Load tick timer interrupt control register to
    * enable interrupts, clear any pending interrupts, 
    * and set the IRQ level.
    */
   *MCC_T4_IRQ_CR  = T4_IRQ_CR_IEN  | 
                     T4_IRQ_CR_ICLR |
                     alm_status.int_pri;

   intUnlock(lock_key);		/* Unlock interrupts */

   ALM_DEBUG_MSG(4,"alm_setup: Counter set to %u ticks.\n",delay,0);
   ALM_DEBUG_MSG(5,"alm_setup: Leaving function.",0,0);
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_reset
 *
 * Description:	This function clears the tick timer counter.
 *
 * Arg(s) In:	enb - flag to enable (1) or disable counting.
 *
 * Arg(s) Out:	None.
 *
 * Side: Always enable counting.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

long alm_reset(int enb)
{

  *MCC_TIMER4_CNT = 0; /* Set the tick timer counter register */
  if (enb)
    *MCC_TIMER4_CR = TIMER4_CR_CEN;    /* enable counting   */
  return(OK); 
}


/*+**************************************************************************
 *
 * Function:	alm_disable
 *
 * Description:	Disable the counter interrupts.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_disable (void)
{
   register int lock_key=0;

   ALM_DEBUG_MSG(5, "alm_disable: Entering function.",0,0);

   if (alm_status.running) {
      lock_key = intLock();	           /* Lock interrupts                */
      /*
       * Clear the tick timer interrupt control register
       * to disable interrupts. Note: clearing the counter
       * enable bit will also disable the counter. However,
       * it is nicer to simply reset this register.
       */
      *MCC_T4_IRQ_CR = 0;                  /* clear the counter enable bit   */
      alm_status.running = FALSE;          /* indicate status at NOT running */
      intUnlock(lock_key);	           /* Unlock interrupts              */

      ALM_DEBUG_MSG(4,"alm_disable: Counter interrupts disabled.",0,0);
   }
   ALM_DEBUG_MSG(5, "alm_disable: Leaving function.",0,0);
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_enable
 *
 * Description:	Enable the timer.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Error code:
 *                           OK or ERROR
 *
 **************************************************************************-*/

long alm_enable(void)
{
     long status=OK;

     ALM_DEBUG_MSG(5, "alm_enable: Entering function.",0,0);

     /* Set the control register to enable the timer. */
     *MCC_TIMER4_CR  = TIMER4_CR_CEN;  /* Set the counter enable bit */

     /* Check for CPU speed */
     if (*(MAGIC_P) == MAGIC_25) {
       int_inhibit  = INT_INHIBIT_25;
       int_subtract = INT_SUBTRACT_25;
     }
     else if (*(MAGIC_P) == MAGIC_32) {
       int_inhibit  = INT_INHIBIT_32;
       int_subtract = INT_SUBTRACT_32;
     } else {
        ALM_DEBUG_MSG(1,"alm_enable: Illegal CPU speed.",0,0);
        status = ERROR;
     }
     /* 
      * ??? Secure PATCH for T2.0.2
      * should be replaced by something real !!!
      */
     if (status==OK) {    
       int_inhibit  = INT_INHIBIT_SAFE;
       int_subtract = INT_SUBTRACT_SAFE;
     }

     ALM_DEBUG_MSG(5,"alm_enable: Leaving function.",0,0);
     return (status);
}


/*+**************************************************************************
 *
 * Function:	alm_freq
 *
 * Description:	Returns the frequency of the alarm clock, in ticks per
 *              second.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	The alarm clock frequency, in ticks per second.
 *
 **************************************************************************-*/

unsigned long alm_freq (void)
{
   unsigned long timer_freq=0;     /* clock frequency                */
   unsigned long bclk = 25000000;  /* default cpu clock speed, 25Mhz */

   ALM_DEBUG_MSG(5,"alm_freq: Entering function.",0,0);

   if (*MCC_VERSION_REG & VERSION_REG_SPEED)	/* check clock speed        */
      bclk = 33000000;                          /* set clock speed to 33Mhz */
   timer_freq = bclk/(256 - (*MCC_PRESCALE_CLK_ADJ & 0xff));

   ALM_DEBUG_MSG(5,"alm_freq: Leaving function.",0,0);
   return (timer_freq);
}


/*+**************************************************************************
 *
 * Function:	alm_int_ack
 *
 * Description:	This function is called in the user defined
 *              interrupt handler to acknowledge the interrupt.
 *
 * Arg(s) In:	None
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_int_ack(void)
{
   register int lock_key=0;
     
   /* Acknowledge the interrupt. */
   lock_key = intLock();   
   *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR;  /* Clear IRQ's */
   intUnlock(lock_key);	  
   return;
}


/*+**************************************************************************
 * 
 * Function:    alm_intLevelGet
 *
 * Description:  This routine returns interrupt vector priority level.
 *              currently set.
 *
 * Arg(s) In:   None
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector priority level (1-7).
 *
 **************************************************************************-*/
 
int alm_intLevelGet(void)
{
    unsigned long level = 0;
       
   /* Read the tick timer interrupt control register 
    * and get the current IRQ priority level set.
    */
   level = *MCC_T4_IRQ_CR & 0xf;
   return((int)level);
}


/*+**************************************************************************
 *
 * Function:	alm_int_delay_adjust
 *
 * Description:	This function is called during alarm initialzation
 *              by the user to obtain the interrupt dealy adjustment.
 *
 * Arg(s) In:	None
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   interrupt delay adjustment value
 *
 **************************************************************************-*/

unsigned long alm_int_delay_adjust(void)
{
    return(int_subtract);
}


