/* $RCSfile: alm_mcc.c,v $ */

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	Timer - Timer and Alarm Clock Support
 *
 * File:	alm_mcc.c
 *
 * Description:	Library package to implement an usec alarm clock.
 *              System dependent part - Motorola MCCchip
 *              On the MOTOROLA MVME 162/167:
 *                Uses the timer 4 on the mvme16x's MCC chip. (See also 
 *		  timestamp timer code in ~vw/src/drv/timer/mccTimerTS.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 1.1 $
 * $Date: 1996/10/29 13:11:53 $
 *
 * $Author: lange $
 *
 * $Log: alm_mcc.c,v $
 * Revision 1.1  1996/10/29 13:11:53  lange
 * First version to go into EPICS tree (locally).
 *
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 **************************************************************************-*/

#include <vxWorks.h>
#include <intLib.h>
#include <iv.h>

#include <drv/multi/mcchip.h>

#include <debugmsg.h>

#include "alm.h"


/*+**************************************************************************
 *		Global Functions implemented here
 **************************************************************************-*/
				/* These are declared in alm.h */
#if 0
/* Return a time stamp */
unsigned long alm_get_stamp (void);

/* Initialize the alarm library */
int alm_init (void);

/* Return the frequency of the alarm clock */
unsigned long alm_freq (void)
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

   status.running = TRUE;	/* Set running flag */

   lock_key = intLock();	/* Lock interrupts */

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

      intUnlock(lock_key);	/* Unlock interrupts */

      status.running = FALSE;
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
 * Function:	alm_init
 *
 * Description:	Initialize the alarm library.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Error code:
 *                           0  -  OK.
 *                          -1  -  Error.
 *
 **************************************************************************-*/

int alm_init (void)
{
static char
rcsid[] = "@(#)mCAN-timer: $Id: alm_mcc.c,v 1.1 1996/10/29 13:11:53 lange Exp $";

   DBG(5, "Entering alm_init.");

   if (!status.init_d) {
      status.init_d = TRUE;

      DBG_INIT;

      if (INIT_LOCK(alm_lock)) { /* Init mutex semaphore */
	 return(-1);
      }
				/* Connect interrupt handler */
      (void) intConnect (INUM_TO_IVEC (MCC_INT_VEC_BASE + MCC_INT_TT4),
			 alm_int_handler, NULL);

				/* Enable the timer */
      *MCC_TIMER4_CR  = TIMER4_CR_CEN;

      DBG(1, "alm: initialization done.");
   }

   DBG(5, "Leaving alm_init.");
   return(0);
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
