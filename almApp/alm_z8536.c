/* $RCSfile: alm_z8536.c,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_z8536.c  -  (included by alm.c)
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Zilog z8536 counter/timer/port
 *
 *              Uses timer 1, 2 & 3 on the User CIO. (See
 *              code in ~vw/src/drv/timer/z8536Timer2.c)
 *
 *		C/T 1 & 2 are linked and used as a 32 bit countdown timer.
 *		C/T 3 is a free running timestamp generator.
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.3 $
 * $Date: 1999/09/08 17:40:22 $
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
#include <taskLib.h>
#include <iv.h>

#include <drv/multi/z8536.h>

#include <debugmsg.h>
#include <almLib.h>

extern void logMsg(char*, ...);


/*+**************************************************************************
 *		Globals
 **************************************************************************-*/

static unsigned long ref_stamp = 0; /* Reference time stamp */


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
unsigned long alm_freq (void)
#endif


/*+**************************************************************************
 *		Local Functions
 **************************************************************************-*/

/* Read from User CIO */
static unsigned char alm_cio_rd (unsigned char reg);

/* Write to User CIO */
static void alm_cio_wr (unsigned char reg, unsigned char val);

/* Set the Zilog 8536 C/T 1/2 to a new count */
static void alm_set_counter_to (unsigned long newcnt);

/* Error interrupt handler (gets called at interrupt overflow) */
static void alm_z8536_err_int (int arg);

/* Interrupt handler for counter 3 (used for timestamp) */
static void alm_z8536_stamp_int (int arg);

/* Interrupt handler for counter 1 (set free run) */
static void alm_z8536_ct1_int (int arg);

/* Setup the timer to interrupt in <delay> microseconds */
static void alm_setup (unsigned long delay);

/* Disable the timer interrupts */
static void alm_disable (void);

/* The alarm counter's interrupt handler */
static void alm_int_handler (int arg);


/*+**************************************************************************
 *
 * Function:	alm_cio_rd
 *
 * Description:	Read from a register of the user CIO
 *
 * Arg(s) In:	reg  -  Number of register to read.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Value of register.
 *
 **************************************************************************-*/

static unsigned char alm_cio_rd (unsigned char reg)
{
   register volatile unsigned char*
      cioCtl = ZCIO2_CNTRL_ADRS; /* Points to command register */   

   *cioCtl = reg;
   return(*cioCtl);
}


/*+**************************************************************************
 *
 * Function:	alm_cio_wr
 *
 * Description:	Write to a register of the user CIO
 *
 * Arg(s) In:	reg  -  Number of register to write to.
 *              val  -  Value to write to register.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

static void alm_cio_wr (unsigned char reg, unsigned char val)
{
   register volatile unsigned char*
      cioCtl = ZCIO2_CNTRL_ADRS; /* Points to command register */   

   *cioCtl = reg;
   *cioCtl = val;
   return;
}


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
   register unsigned long currcnt;  /* Current timer count */
   register unsigned short stat0;   /* Timer status */
   register unsigned short stat1;
   int lock_key;
#ifdef DEBUGMSG
   unsigned long tmp;
   unsigned char s1, s2;

   if (!intContext())		/* Lock task if not in interrupt context */
      (void) taskLock();
#endif

   lock_key = intLock();	/* Lock interrupts */

   do {
      stat0 = alm_cio_rd(ZCIO_CT3CS);
				/* Freeze counters */
      alm_cio_wr(
	 ZCIO_CT3CS,		/* C/T 3 Command and Status */
	 ZCIO_CS_GCB		/* Gate Command Bit */
	 | ZCIO_CS_RCC);	/* freeze Current-Count-Reg. */

				/* Read Counters */
				/* Read C/T 3 CCR MSB */
      currcnt =  ((unsigned long) alm_cio_rd(ZCIO_CT3CCMSB) <<  8);
				/* Read C/T 3 CCR LSB */
      currcnt |=  (unsigned long) alm_cio_rd(ZCIO_CT3CCLSB);
				/* (after reading LSB the CCR */
				/* is running again) */
      stat1 = alm_cio_rd(ZCIO_CT3CS);

#ifdef DEBUGMSG
      tmp = currcnt;		/* Remember ... */
#endif
				/* Convert to microseconds
				   considering pending interrupts */
      currcnt = ref_stamp + ((65536ul - currcnt) << 1) / 5;
      
      if (stat0 & ZCIO_CS_IP) {
				/* One was pending ... */
	 currcnt += 26214ul;
	 break;
      } else if (stat1 & ZCIO_CS_IP)
				/* Oops ... this interrupt was locked out */
	 continue;
      else {
				/* Nothing happened so far */
	 break;
      }
   } while (TRUE);
      
#ifdef DEBUGMSG
   s1 = alm_cio_rd(ZCIO_CT3CS);	/* C/T 3 Command and Status */
   s2 = alm_cio_rd(ZCIO_CT3MS);	/* C/T 3 Mode */

   PRF(5, ("alm_get_stamp: Status 3 = 0x%2x Mode 3 = 0x%2x.\n", s1, s2));

   s1 = alm_cio_rd(ZCIO_MCC);	/* Read CIO 2 MCC register */
   s2 = alm_cio_rd(ZCIO_MIC);	/* Read CIO 2 MIC register */

   PRF(5, ("alm_get_stamp: MIC = 0x%2x MCC = 0x%2x.\n", s2, s1));
   PRF(4, ("alm_get_stamp: Reference stamp %lu.\n", ref_stamp));
#endif
   /* Stamp = Ref_Stamp + [(Last_Setup - Currcnt) * 0.4] */

   
   intUnlock(lock_key);		/* Unlock interrupts */

   PRF(3, ("alm_get_stamp: Timer 0x%lx Stamp %lu.\n",
	   tmp, currcnt));

#ifdef DEBUGMSG
   if (!intContext())		/* Unlock task if not in interrupt context */
      (void) taskUnlock();
#endif

   return(currcnt);
}


/*+**************************************************************************
 *
 * Function:	alm_set_counter_to
 *
 * Description:	Set the Zilog 8536 C/T 1/2 to a new count.
 *              Interrupt must be locked to use this function.
 *
 * Arg(s) In:	newcnt  -  Value to set the counters to.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_set_counter_to (unsigned long newcnt)
{
#ifdef DEBUGMSG
   register unsigned long currcnt; /* Current count */
   unsigned char s1, s2;
#endif

   DBG(5, "Entering alm_set_counter_to.");

				/* Stop counters */

   alm_cio_wr(
      ZCIO_CT2CS,	      /* C/T 2 Command and Status */
      0);			/* Stop */
   alm_cio_wr(
      ZCIO_CT1CS,	      /* C/T 1 Command and Status */
      0);			/* Stop */

   newcnt += 0x00010001ul;	/* 1->0 raises interrupt */

				/* Load new Time Const registers */
   alm_cio_wr(
      ZCIO_CT2TCMSB,	      /* C/T 2 Time Const MSB */
      (unsigned char) (newcnt >> 24));
   alm_cio_wr(
      ZCIO_CT2TCLSB,	      /* C/T 2 Time Const LSB */
      (unsigned char) (newcnt >> 16));
   alm_cio_wr(
      ZCIO_CT1TCMSB,	      /* C/T 1 Time Const MSB */
      (unsigned char) 0);
   alm_cio_wr(
      ZCIO_CT1TCLSB,	      /* C/T 1 Time Const LSB */
      (unsigned char) 1);

				/* Start and trigger counters */
   alm_cio_wr(
      ZCIO_CT2CS,	      /* C/T 2 Command and Status */
      ZCIO_CS_SIE		/* Set Interrupt Enable */
      | ZCIO_CS_GCB		/* Gate Command Bit */
      | ZCIO_CS_TCB);		/* Trigger Command Bit */

   alm_cio_wr(
      ZCIO_CT1MS,	      /* C/T 1 Mode */
      0);			/* Single Cycle */

   alm_cio_wr(
      ZCIO_CT1CS,	      /* C/T 1 Command and Status */
      ZCIO_CS_GCB		/* Gate Command Bit */
      | ZCIO_CS_CLIE		/* CLear Interrupt Enable */
      | ZCIO_CS_TCB);		/* Trigger Command Bit */

   /* Now C/T 1 runs down to trigger C/T 2 */

   alm_cio_wr(
      ZCIO_CT1TCMSB,	      /* C/T 1 Time Const MSB */
      (unsigned char) (newcnt >> 8));
   alm_cio_wr(
      ZCIO_CT1TCLSB,	      /* C/T 1 Time Const LSB */
      (unsigned char) newcnt);

   alm_cio_wr(		      /* Clear the interrupt registers */
      ZCIO_CT1CS,		/* C/T 1 Command and Status */
      ZCIO_CS_CLIPIUS);		/* Clear IP and IUS */

				/* Start C/T 1 again */
   alm_cio_wr(
      ZCIO_CT1CS,	      /* C/T 1 Command and Status */
      ZCIO_CS_SIE		/* Set Interrupt Enable */
      | ZCIO_CS_GCB		/* Gate Command Bit */
      | ZCIO_CS_TCB);		/* Trigger Command Bit */

#ifdef DEBUGMSG
				/* Freeze counters */
   alm_cio_wr(
      ZCIO_CT1CS,	      /* C/T 1 Command and Status */
      ZCIO_CS_GCB		/* Gate Command Bit */
      | ZCIO_CS_RCC);		/* freeze Current-Count-Reg. */
   alm_cio_wr(
      ZCIO_CT2CS,	      /* C/T 2 Command and Status */
      ZCIO_CS_GCB		/* Gate Command Bit */
      | ZCIO_CS_RCC);		/* freeze Current-Count-Reg. */

				/* Read Counters */

				/* Read C/T 2 CCR MSB */
   currcnt =  ((unsigned long) alm_cio_rd(ZCIO_CT2CCMSB) <<  24);
				/* Read C/T 2 CCR LSB */
   currcnt |= ((unsigned long) alm_cio_rd(ZCIO_CT2CCLSB) <<  16);
				/* Read C/T 1 CCR MSB */
   currcnt |= ((unsigned long) alm_cio_rd(ZCIO_CT1CCMSB) <<  8);
				/* Read C/T 1 CCR LSB */
   currcnt |=  (unsigned long) alm_cio_rd(ZCIO_CT1CCLSB);
				/* (after reading LSB the CCR */
				/* is running again) */

   s1 = alm_cio_rd(ZCIO_CT1CS);	/* C/T 1 Command and Status */
   s2 = alm_cio_rd(ZCIO_CT2CS);	/* C/T 2 Command and Status */

   PRF(4, ("alm_set_counter_to: Status 2 = 0x%2x, Status 1 = 0x%2x.\n",
	   s2, s1));
   PRF(3, ("alm_set_counter_to: C/T 1/2 set to 0x%lx, now 0x%lx.\n",
	   newcnt, currcnt));
#endif

   DBG(5, "Leaving alm_set_counter_to.");
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_z8536_err_int
 *
 * Description:	Interrupt handler (gets called at counter overflow).
 *
 * Arg(s) In:	arg  -  Standard interrupt handler argument.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_z8536_err_int (int arg)
{
   int lock_key;		/* Interrupt lock key */
   register char ctcs;		/* C/T command and status */

   DBG_ENTER_INT_CONTEXT;
   lock_key = intLock ();	/* Lock interrupts */

   /* Check if C/T 1 error bit set */
				/* C/T 1 Command and Status */
   ctcs = alm_cio_rd(ZCIO_CT1CS);

   if (ctcs & (ZCIO_CS_IP | ZCIO_CS_ERR)) {
      intUnlock(lock_key);	/* Unlock interrupts */

      logMsg ("ERR INT CT1 handler !!!\n"); /* *** */

      alm_z8536_ct1_int(arg);
   }

   /* Check if C/T 2 error bit set */
				/* C/T 2 Command and Status */
   ctcs = alm_cio_rd(ZCIO_CT2CS);

   if (ctcs & (ZCIO_CS_IP | ZCIO_CS_ERR)) {
      intUnlock(lock_key);	/* Unlock interrupts */

      logMsg ("ERR INT CT2 handler !!!\n"); /* *** */

      alm_int_handler(arg);
   }

   /* Check if C/T 3 error bit set */
				/* C/T 3 Command and Status */
   ctcs = alm_cio_rd(ZCIO_CT3CS);

   if (ctcs & (ZCIO_CS_IP | ZCIO_CS_ERR)) {
      intUnlock(lock_key);	/* Unlock interrupts */

      logMsg ("ERR INT CT3 handler !!!\n"); /* *** */

      alm_z8536_stamp_int(arg);
   }
   
   DBG_LEAVE_INT_CONTEXT;
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_z8536_stamp_int
 *
 * Description:	Interrupt handler for counter 3 (used for timestamp).
 *
 * Arg(s) In:	arg  -  Standard interrupt handler argument.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_z8536_stamp_int (int arg)
{
   register int lock_key;	 /* Interrupt lock key */

   DBG_ENTER_INT_CONTEXT;
   lock_key = intLock();	/* Lock interrupts */

   alm_cio_wr(		      /* Acknowledge the interrupt */
      ZCIO_CT3CS,		/* C/T 3 Command and Status */
      ZCIO_CS_CLIPIUS		/* Clear IP and IUS */
      | ZCIO_CS_GCB);		/* Gate Command Bit */

   ref_stamp += 26214ul;	/* Increase reference stamp */

   intUnlock(lock_key);		/* Unlock interrupts */
   DBG_LEAVE_INT_CONTEXT;
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_z8536_ct1_int
 *
 * Description:	Interrupt handler for counter 1.
 *              Disables interrupts and sets up free run.
 *
 * Arg(s) In:	arg  -  Standard interrupt handler argument.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_z8536_ct1_int (int arg)
{
   register int lock_key;	 /* Interrupt lock key */

   DBG_ENTER_INT_CONTEXT;
   lock_key = intLock ();	/* Lock interrupts */

#if 0
   logMsg ("INT CT1!!!\n");	/* *** */
#endif

   alm_cio_wr(		      /* Acknowledge the interrupt */
      ZCIO_CT1CS,		/* C/T 1 Command and Status */
      0				/* Stop */
      | ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */

				/* C/T 2 Command and Status */
   if ((alm_cio_rd(ZCIO_CT2CS) & ZCIO_CS_IP) == 0)
   {
      alm_cio_wr(
	 ZCIO_CT1MS,		/* C/T 1 Mode */
	 ZCIO_CTMS_CSC);	/* Continuous Cycle */

      alm_cio_wr(	      /* Load C/T 1 TC for free run */
	 ZCIO_CT1TCMSB,		/* C/T 1 Time Const MSB */
	 0);
      alm_cio_wr(
	 ZCIO_CT1TCLSB,		/* C/T 1 Time Const LSB */
	 0);

      alm_cio_wr(	      /* Start C/T 1 again */
	 ZCIO_CT1CS,		/* C/T 1 Command and Status */
	 ZCIO_CS_CLIE		/* CLear Interrupt Enable */
	 | ZCIO_CS_GCB		/* Gate Command Bit */
	 | ZCIO_CS_TCB);	/* Trigger Command Bit */
      
   }
   intUnlock(lock_key);		/* Unlock interrupts */
   DBG_LEAVE_INT_CONTEXT;
   return;
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

				/* Convert delay to ticks */
   if (delay < INT_INHIBIT)
      delay = (INT_INHIBIT >> 1) * 5;    /* Min. value */
   else if (delay > 1717986918ul)
      delay = 0;		        /* Max. value */
   else if (delay & 0x00000001ul)
      delay = ((delay >> 1) * 5) + 3ul; /* 1 / 0.4 */
   else
      delay = (delay >> 1) * 5;

   lock_key = intLock();	/* Lock interrupts */

   alm_set_counter_to(delay);

   intUnlock(lock_key);		/* Unlock interrupts */

   PRF(4, ("alm_setup: counter set to 0x%8x ticks.\n", delay));
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

				/* Stop counters */
      alm_cio_wr(
	 ZCIO_CT2CS,	      /* C/T 2 Command and Status */
	 0			/* Stop */
	 | ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */
      alm_cio_wr(
	 ZCIO_CT1CS,	      /* C/T 1 Command and Status */
	 0			/* Stop */
	 | ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */

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
#ifdef DEBUGMSG
   register unsigned long currcnt;
   unsigned char s1, s2;
#endif

   DBG_ENTER_INT_CONTEXT;
   lock_key = intLock();	/* Lock interrupts */

#if 0
   logMsg("INT CT2!!!!\n");	/* *** */
#endif

   logMsg("INT CT2 - Timestamp: %lu.\n", alm_get_stamp());
   

   alm_cio_wr(		      /* Acknowledge the interrupt */
      ZCIO_CT2CS,		/* C/T 2 Command and Status */
      ZCIO_CS_CLIPIUS);		/* Clear IP and IUS */

#ifdef DEBUGMSG
				/* Stop and freeze counters */
   alm_cio_wr(
      ZCIO_CT1CS,	      /* C/T 1 Command and Status */
      ZCIO_CS_RCC);		/* freeze Current-Count-Reg. */
   alm_cio_wr(
      ZCIO_CT2CS,	      /* C/T 2 Command and Status */
      ZCIO_CS_RCC);		/* freeze Current-Count-Reg. */

				/* Read Counters */

				/* Read C/T 2 CCR MSB */
   currcnt =  ((unsigned long) alm_cio_rd(ZCIO_CT2CCMSB) <<  24);
				/* Read C/T 2 CCR LSB */
   currcnt |= ((unsigned long) alm_cio_rd(ZCIO_CT2CCLSB) <<  16);
				/* Read C/T 1 CCR MSB */
   currcnt |= ((unsigned long) alm_cio_rd(ZCIO_CT1CCMSB) <<  8);
				/* Read C/T 1 CCR LSB */
   currcnt |=  (unsigned long) alm_cio_rd(ZCIO_CT1CCLSB);
				/* (after reading LSB the CCR */
				/* is running again) */

   s1 = alm_cio_rd(ZCIO_CT1CS);	/* C/T 1 Command and Status */
   s2 = alm_cio_rd(ZCIO_CT2CS);	/* C/T 2 Command and Status */

   PRF(4, ("alm_int_handler: Status 2 = 0x%2x, Status 1 = 0x%2x.\n",
	   s2, s1));
   PRF(3, ("alm_int_handler: C/T 1/2 is 0x%lx.\n",
	   currcnt));
#endif

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
 *                           0  -  OK.
 *                          -1  -  Error.
 *
 **************************************************************************-*/

long almInit (void)
{
static char
rcsid[] = "@(#)almLib: $Id: alm_z8536.c,v 2.3 1999/09/08 17:40:22 lange Exp $";

   register int lock_key;
   register unsigned char tmp;    /* Dummy */

   DBG(5, "Entering almInit.");

   if (!status.init_d) {
      status.init_d = TRUE;

      DBG_INIT;

      if (INIT_LOCK(alm_lock)) { /* Init mutex semaphore */
	 return(-1);
      }

      lock_key = intLock();	/* Lock interrupts */

			        /* read CIO 2 MCC register */
      tmp = alm_cio_rd(ZCIO_MCC);
      alm_cio_wr(
	 ZCIO_MCC,		/* and disable C/T 1 2 3 */
	 tmp
	 & ~(ZCIO_MCC_CT1E |
	     ZCIO_MCC_CT2E |
	     ZCIO_MCC_CT3PCE));

      alm_cio_wr(
	 ZCIO_CTIV,		/* Set Interrupt Vector */
	 INT_VEC_BASE_CIO2);

      alm_cio_wr(
	 ZCIO_CT1MS,	      /* C/T 1 Mode */
	 0);			/* Single Cycle */

      alm_cio_wr(
	 ZCIO_CT2MS,	      /* C/T 2 Mode */
	 0);			/* Single Cycle */

      alm_cio_wr(
	 ZCIO_CT3MS,	      /* C/T 3 Mode */
	 ZCIO_CTMS_CSC);	/* Continuous Cycle */

      alm_cio_wr(
	 ZCIO_CT1CS,	      /* C/T 1 Command and Status */
	 ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */
      alm_cio_wr(
	 ZCIO_CT2CS,	      /* C/T 2 Command and Status */
	 ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */
      alm_cio_wr(
	 ZCIO_CT3CS,	      /* C/T 3 Command and Status */
	 ZCIO_CS_CLIPIUS);	/* Clear IP and IUS */

			        /* read CIO 2 MCC register */
      tmp = alm_cio_rd(ZCIO_MCC);
      alm_cio_wr(
	 ZCIO_MCC,		/* Setup timer link */
	 tmp
	 |= ZCIO_MCC_LC_CNT);	/* ct1 output is ct2 CouNT input */
      alm_cio_wr(
	 ZCIO_MCC,		/* and enable C/T 1 2 3 */
	 tmp
	 | ZCIO_MCC_CT1E
	 | ZCIO_MCC_CT2E
	 | ZCIO_MCC_CT3PCE);

				/* read CIO 2 MIC register */
      tmp = alm_cio_rd(ZCIO_MIC);
      alm_cio_wr(
	 ZCIO_MIC,
	 tmp
	 | ZCIO_MIC_CTVIS);	/* enable CT VIS */

				/* Start and trigger counter 3 */
      alm_cio_wr(
	 ZCIO_CT3TCMSB,	      /* C/T 3 Time Const MSB */
	 (unsigned char) 0);
      alm_cio_wr(
	 ZCIO_CT3TCLSB,	      /* C/T 3 Time Const LSB */
	 (unsigned char) 0);
      alm_cio_wr(
	 ZCIO_CT3CS,	      /* C/T 3 Command and Status */
	 ZCIO_CS_SIE		/* Set Interrupt Enable */
	 | ZCIO_CS_GCB		/* Gate Command Bit */
	 | ZCIO_CS_TCB);	/* Trigger Command Bit */

				/* Connect interrupt handler */
      (void) intConnect (INUM_TO_IVEC(INT_VEC_CT2_1),
			 alm_z8536_ct1_int, NULL);

      (void) intConnect (INUM_TO_IVEC(INT_VEC_CT2_2),
			 alm_int_handler, NULL);

      (void) intConnect (INUM_TO_IVEC(INT_VEC_CT2_3),
			 alm_z8536_stamp_int, NULL);

				/* Connect error handler */
      (void) intConnect (INUM_TO_IVEC (INT_VEC_CT2_ERR),
			 alm_z8536_err_int, NULL);

      intUnlock(lock_key);

      DBG(1, "alm: initialization done.");
   }
   DBG(5, "Leaving almInit.");
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
   return (ZCIO_HZ);
}

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_z8536.c  -  (included by alm.c)
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *              System dependent part - Zilog z8536 counter/timer/port
 *
 *              Uses timer 1, 2 & 3 on the User CIO. (See
 *              code in ~vw/src/drv/timer/z8536Timer2.c)
 *
 *		C/T 1 & 2 are linked and used as a 32 bit countdown timer.
 *		C/T 3 is a free running timestamp generator.
 *
 * Author(s):	Ralph Lange
 *
 * $Log: alm_z8536.c,v $
 * Revision 2.3  1999/09/08 17:40:22  lange
 * Fixed Tornado101 warnings
 *
 * Revision 2.2  1997/07/31 18:07:10  lange
 * alm -> almLib
 *
 * Revision 2.1  1997/07/31 17:49:50  lange
 * alm_init -> almInit.
 *
 * Revision 2.0  1997/02/07 16:30:51  lange
 * Changed interface; alm is standalone module now.
 *
 * Revision 1.2  1997/02/07 16:04:47  lange
 * Added counter increment; made alm a module of its own.
 *
 * Revision 1.1  1996/10/29 13:11:54  lange
 * First version to go into EPICS tree (locally).
 *
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/
