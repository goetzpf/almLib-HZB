/* $RCSfile: almLib.h,v $ */

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	Timer - Timer and Alarm Clock Support
 *
 * File:	alm.h
 *
 * Description:	Library package to implement an alarm clock using the
 *              timer 4 on the mvme162'c MCC chip.
 *		(See also timestamp timer code in vw/src/drv/timer/xxxTS.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 1.3 $
 * $Date: 1996/06/03 20:18:34 $
 *
 * $Author: lange $
 *
 * $Log: almLib.h,v $
 * Revision 1.3  1996/06/03 20:18:34  lange
 * Alarms seem to work now (multiple bugs fixed).
 *
 * Revision 1.2  1996/05/22 14:09:44  lange
 * New watchdog-like functionality.
 *
 * Revision 1.1  1996/05/20 11:56:01  lange
 * Changed name (to avoid EPICS name conflicts).
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 **************************************************************************-*/


#ifndef __ALM_H
#define __ALM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <vxWorks.h>
#include <semaphore.h>


/*+**************************************************************************
 *		Types
 **************************************************************************-*/
   
typedef void* alm_ID;
   

/*+**************************************************************************
 *		Functions
 **************************************************************************-*/

extern int
alm_init (void);	     /* Initialize library */

extern unsigned long
alm_get_stamp (void);	     /* Get a time stamp (1 us resolution) */

extern alm_ID
alm_start (		     /* Set up an alarm */
   unsigned long delay,		/* Alarm delay in us */
   sem_t* sem_p			/* Semaphore to post on alarm */
   );

extern void
alm_cancel (		     /* Cancel a running alarm */
   alm_ID id			/* ID of alarm to cancel */
   );

extern UINT32
alm_freq (void);	     /* Return the alarm clock frequency */



/*+**************************************************************************
 *		Debug Mode
 **************************************************************************-*/

#ifdef ALM_DEBUG

extern int almSetDebug (char verb);

extern void			/* Print alarms */
almInfo (unsigned char);

#endif


#ifdef __cplusplus
}
#endif

#endif /* ifndef __ALM_H */
