/* $RCSfile: alm_mcc.c,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_mcc.c  -  (included by alm.c)
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Motorola MCCchip
 *              On the MOTOROLA MVME 162/167:
 *                Uses the timer 4 on the mvme16x's MCC chip. (See also 
 *		  timestamp timer code in ~vw/src/drv/timer/mccTimerTS.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.4 $
 * $Date: 2004/03/29 13:29:14 $
 *
 * $Author: lange $
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

#include <vxWorks.h>
#include <intLib.h>
#include <iv.h>

#include <drv/multi/mcchip.h>

#include <debugmsg.h>
#include <almLib.h>

				/* Definitions for 25 MHz CPU */
#define INT_INHIBIT_25  25ul	/* Minimum interrupt delay */
#define INT_SUBTRACT_25 46ul	/* Subtracted from every delay */

				/* Definitions for 32 MHz CPU */
#define INT_INHIBIT_32 20ul	/* Minimum interrupt delay */
#define INT_SUBTRACT_32 34ul	/* Subtracted from every delay */

                                /* Safe values (prelimininary patch) */
#define INT_INHIBIT_SAFE 50ul
#define INT_SUBTRACT_SAFE 0ul

				/* Magic Words */
#define MAGIC_P  (unsigned long*)(0xfffc1f28)	/* Address of Magic Word */
#define MAGIC_25 0x32353030ul	/* ASCII 2500 */
#define MAGIC_32 0x33323030ul	/* ASCII 3200 */


/*+**************************************************************************
 *		Global Functions implemented here
 **************************************************************************-*/
				/* These are declared in almLib.h */
#if 0
/* Return a time stamp */
unsigned long alm_get_stamp (void);

/* Initialize the alarm library */
long almInit (void);

/* Return the frequency of the alarm clock */
unsigned long alm_freq (void);
#endif

/*+**************************************************************************
 *		Local Functions
 **************************************************************************-*/

/* Setup the timer to interrupt in <delay> microseconds */
static void alm_setup (unsigned long delay);

/* Disable the timer interrupts */
static void alm_disable (void);

/* The alarm counter's interrupt handler */
static void alm_int_handler (int arg);

/*+**************************************************************************
 *		Globals
 **************************************************************************-*/

static unsigned long int_inhibit;
static unsigned long int_subtract;


/*+**************************************************************************
 *
 * Function:	alm_get_stamp
 *
 * Description:	Returns a time stamp from the alarm timer (32 bit;
 *              1 us resolution)
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
   register int lock_key;

   DBG(5, "Entering alm_setup.");

   lock_key = intLock();	/* Lock interrupts */

   status.running = TRUE;	/* Set running flag */

				/* Set the timer compare value */
   *MCC_TIMER4_CMP = *MCC_TIMER4_CNT + 
      (delay < INT_INHIBIT ? INT_INHIBIT : delay);

				/* Reset & enable the alarm timer interrupt */
   *MCC_T4_IRQ_CR  = T4_IRQ_CR_IEN | T4_IRQ_CR_ICLR | ALARMCLOCK_LEVEL;

   intUnlock(lock_key);		/* Unlock interrupts */

   PRF(4, ("alm_setup: counter set to %u ticks.\n", delay));
   DBG(5, "Leaving alm_setup.");
   return;
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
   register int lock_key;

   DBG(5, "Entering alm_disable.");

   if (status.running) {

      lock_key = intLock();	/* Lock interrupts */

      *MCC_T4_IRQ_CR = 0;	/* Disable interrupts */

      status.running = FALSE;

      intUnlock(lock_key);	/* Unlock interrupts */

      DBG(4, "alm_disable: counter interrupts disabled.");
   }

   DBG(5, "Leaving alm_disable.");
}


/*+**************************************************************************
 *
 * Function:	alm_int_handler
 *
 * Description:	Interrupt handler (gets called at counter overflow).
 *              The counter is disabled and a user routine is called (if it 
 *              was connected by alm_start).
 *
 * Arg(s) In:	arg  -  Standard interrupt handler argument.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_int_handler (int arg)
{
   register int lock_key;

   DBG_ENTER_INT_CONTEXT;

   lock_key = intLock();	/* Lock interrupts */

   *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR; 	/* Acknowledge timer interrupt */

   intUnlock(lock_key);		/* Unlock interrupt */

   if (status.in_use) {		/* User call is active */

      status.woke_up = TRUE;
      DBG(3, "Handler woke up.");

   } else			/* No users around */

      alm_check();		/* Check for and post alarms */

   DBG_LEAVE_INT_CONTEXT;
}


/*+**************************************************************************
 *
 * Function:	almInit
 *
 * Description:	Initialize the alarm library.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Error code:
 *                           OK or ERROR
 *
 **************************************************************************-*/

long almInit (void)
{
static char
rcsid[] = "@(#)almLib: $Id: alm_mcc.c,v 2.4 2004/03/29 13:29:14 lange Exp $";

   DBG(5, "Entering almInit.");

   if (!status.init_d) {
      status.init_d = TRUE;

      DBG_INIT;

      if (INIT_LOCK(alm_lock)) { /* Init mutex semaphore */
	 return ERROR;
      }
				/* Connect interrupt handler */
      (void) intConnect (INUM_TO_IVEC (MCC_INT_VEC_BASE + MCC_INT_TT4),
			 alm_int_handler, NULL);

				/* Enable the timer */
      *MCC_TIMER4_CR  = TIMER4_CR_CEN;

				/* Check for CPU speed */
      if (*(MAGIC_P) == MAGIC_25) {
	 int_inhibit  = INT_INHIBIT_25;
	 int_subtract = INT_SUBTRACT_25;
      }
      else if (*(MAGIC_P) == MAGIC_32) {
	 int_inhibit  = INT_INHIBIT_32;
	 int_subtract = INT_SUBTRACT_32;
      } else {
	 DBG(1, "alm: Illegal CPU speed.");
	 return ERROR;
      }

/* ??? Secure PATCH for T2.0.2 - should be replaced by something real !!! */
	 int_inhibit  = INT_INHIBIT_SAFE;
	 int_subtract = INT_SUBTRACT_SAFE;

      DBG(1, "alm: initialization done.");
   }

   DBG(5, "Leaving almInit.");
   return OK;
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
   unsigned long timer_freq;
   unsigned long bclk;

   DBG(5, "Entering alm_freq.");

   if (*MCC_VERSION_REG & VERSION_REG_SPEED)	/* check clock speed */
      bclk = 33000000;
   else
      bclk = 25000000;

   timer_freq = bclk/(256 - (*MCC_PRESCALE_CLK_ADJ & 0xff));

   DBG(5, "Leaving alm_freq.");
   return (timer_freq);
}

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_mcc.c  -  (included by alm.c)
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Motorola MCCchip
 *              On the MOTOROLA MVME 162/167:
 *                Uses the timer 4 on the mvme16x's MCC chip. (See also 
 *		  timestamp timer code in ~vw/src/drv/timer/mccTimerTS.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Log: alm_mcc.c,v $
 * Revision 2.4  2004/03/29 13:29:14  lange
 * Bugfix: Guard bitfield operations
 *
 * Revision 2.3  1999/09/08 17:40:22  lange
 * Fixed Tornado101 warnings
 *
 * Revision 2.2  1997/07/31 18:07:07  lange
 * alm -> almLib
 *
 * Revision 2.1  1997/07/31 17:50:25  lange
 * alm_init -> almInit, reflects mv162 cpu speed.
 *
 * Revision 2.0  1997/02/07 16:30:49  lange
 * Changed interface; alm is standalone module now.
 *
 * Revision 1.2  1997/02/07 16:04:36  lange
 * Added counter increment; made alm a module of its own.
 *
 * Revision 1.1  1996/10/29 13:11:53  lange
 * First version to go into EPICS tree (locally).
 *
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/
