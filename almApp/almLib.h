/* $RCSfile: almLib.h,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm.h
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		The mv162 implementation uses the timer 4 on the mvme162's
 *		MCC chip (see alm_mcc.c).
 *		The vmod60/eltec27 implementation uses three timers of the
 *		User CIO chip z8536 (see alm_z8536.c).
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.1 $
 * $Date: 1997/07/31 17:47:33 $
 *
 * $Author: lange $
 *
 * $Log: almLib.h,v $
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
 * Copyright (c) 1996, 1997
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/


#ifndef __ALM_H
#define __ALM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <vxWorks.h>
#include <semaphore.h>

#include <debugmsg.h>


/*+**************************************************************************
 *		Types
 **************************************************************************-*/
   
typedef void* alm_ID;		/* Opaque alarm handle type */
   

/*+**************************************************************************
 *		Functions
 **************************************************************************-*/

extern long
almInit (void);			/* Initialize library */

extern unsigned long
alm_get_stamp (void);	     /* Get a time stamp (1 us resolution) */

extern alm_ID
alm_start (		     /* Set up an alarm */
   unsigned long  delay,	/* Alarm delay in us */
   sem_t*         sem_p,	/* Semaphore to post on alarm */
   unsigned long* cnt_p		/* Counter to increment on alarm */
   );

extern void
alm_cancel (		     /* Cancel a running alarm */
   alm_ID id			/* ID of alarm to cancel */
   );

extern unsigned long
alm_freq (void);	     /* Return the alarm clock frequency */

extern void		     /* Print alarm info */
almShow (
   unsigned char v		/* Verbosity [0..1] */
   );


DBG_EXTERN(alm)		     /* Debug messages */


#ifdef __cplusplus
}
#endif

#endif /* ifndef __ALM_H */
