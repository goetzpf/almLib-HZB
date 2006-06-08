/* $RCSfile: almLib.h,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	almLib.h
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		The mv162 implementation uses the timer 4 on the mvme162's
 *		MCC chip (see alm_mcc.c).
 *		The vmod60/eltec27 implementation uses three timers of the
 *		User CIO chip z8536 (see alm_z8536.c).
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.6.2.1 $
 * $Date: 2006/06/08 12:43:58 $
 *
 * $Author: franksen $
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


#ifndef __ALMLIB_H
#define __ALMLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vxWorks.h>        
#include <semaphore.h>        /* sem_t                             */
#include <debugmsg.h>         /* DBG_EXTERN, macro                 */
#include <logLib.h>           /* logMsg() prototype                */

/*+**************************************************************************
 *		Types
 **************************************************************************-*/
   
typedef void* alm_ID;		/* Opaque alarm handle type */
   
/*+**************************************************************************
 *		Functions
 **************************************************************************-*/

extern int almLibIsPrimitive;

extern long
almInit (                    /* Initialize the alarm library. */
   void
   );

extern alm_ID
alm_start (		     /* Set up an alarm */
   unsigned long  delay,	/* Alarm delay in us */
   sem_t*         sem_p,	/* Semaphore to post on alarm */
   unsigned long* cnt_p		/* Counter to increment on alarm */
   );

extern void
alm_check (                  /* Checks alarm list for alarms */
   void
   );

extern void
alm_cancel (                 /* Cancel a running alarm */
   alm_ID id                    /* ID of alarm to cancel */
   );

extern void                  /* Interrupt service routine     */
alm_int_handler (
   int arg
   );
    
extern void		     /* Print alarm info */
almShow (
   unsigned char v		/* Verbosity [0..1] */
   );

extern void                 /* Print alarm info from alm_status global */
alm_status_show (
   unsigned char v             /* Verbosity [0..1] */
    );
    
extern long                 /* Set the interrupt priority level global */
alm_intLevelSet(
    int level                  /* Interrupt priority level [0..7] */
    );

/*+**************************************************************************
 *
 *		Wrapper Functions for low-level timer functions 
 *                     (BSP specific) 
 *
 *               Note: only a few of these low-level timer 
 *                     functions are exported for the general
 *                     use of the application program so that 
 *                     we don't change any existing applcations.
 *                     However, the use of the timer low-level
 *                     function should only be allowed by the
 *                     almLib functions.
 **************************************************************************-*/

/* 
 * The purpose of this function is to call the low=level
 * timer funtion that returns the current the a 32-bit 
 * timestamp.
 */
extern unsigned long   
alm_get_stamp(
   void
   );

/* 
 * This purpose of this function is to call the low-level
 * timer function that will setup the timer counter with
 * the number counts that represent the delay specified 
 * in microseconds and then enable counting.
 */
extern void 
alm_setup(
   unsigned long delay          /* delay in us */
   );

/* 
 * This purpose of this function is to call the low-level timer function
 * that resets the timer comparator counter to 0 for the mv162.
 * Howecver, for the powerPc the low-level timer function sets 
 * the decrementer counter to the maximum value and disables counting.
 */ 
extern long 
alm_reset(
   int enb                    /* Enable or disable counting --- not used */
   ); 

/*
 * The purpose of this function is to return the 
 * interrupt vector table offset used for timer #3.
 * However, this does not indicate it the alarm interrupt
 * handler (alm_int_handler) has been connected to this
 * vector at the time of the call. To determine this the
 * user should invoke the EPICS function veclist(), which 
 * lists all of the functions currently connected to the
 * vectors.
 */

extern unsigned long
alm_intVecGet(
   void
   );

/* 
 * The purpose of this function is to return the interrupt
 * level currently set in the timer#3 register. An interrupt
 * level of 0 indicates that interrupts are disabled.
 */
extern int
alm_intLevelGet(
   void
   );
   
   DBG_EXTERN(alm)		     /* Debug messages */
#ifdef __cplusplus
}
#endif

#endif /* ifndef __ALMLIB_H */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	almLib.h
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		The mv162 implementation uses the timer 4 on the mvme162's
 *		MCC chip (see alm_mcc.c).
 *		The vmod60/eltec27 implementation uses three timers of the
 *		User CIO chip z8536 (see alm_z8536.c).
 *
 * Author(s):	Ralph Lange
 *
 * $Log: almLib.h,v $
 * Revision 2.6.2.1  2006/06/08 12:43:58  franksen
 * alternatively use an extremely simple implementation that can only be used by one task
 *
 * Revision 2.6  2004/07/13 13:14:24  luchini
 * Add wrapper functions for low-level timer support
 *
 * Revision 2.5  2004/06/24 11:22:36  luchini
 * rename almStatus to alm_status_show
 *
 * Revision 2.4  2004/06/22 09:24:14  luchini
 * moved bsp specific prototypes to alm.h
 *
 * Revision 2.3  1999/09/08 17:40:22  lange
 * Fixed Tornado101 warnings
 *
 * Revision 2.2  1997/07/31 18:07:05  lange
 * alm -> almLib
 *
 * Revision 2.1  1997/07/31 17:47:33  lange
 * Changed alm_init to almInit.
 *
 * Revision 2.0  1997/02/07 16:30:48  lange
 * Changed interface; alm is standalone module now.
 *
 * Revision 1.5  1997/02/07 16:04:35  lange
 * Added counter increment; made alm a module of its own.
 *
 * Revision 1.4  1996/10/29 13:16:17  lange
 * First version to go into EPICS tree (locally).
 *
 * Revision 1.3  1996/06/03 20:18:34  lange
 * Alarms seem to work now (multiple bugs fixed).
 *
 * Revision 1.2  1996/05/22 14:09:44  lange
 * New watchdog-like functionality.
 *
 * Revision 1.1  1996/05/20 11:56:01  lange
 * Changed name (to avoid EPICS name conflicts).
 *
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/
