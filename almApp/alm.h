/* alm.h */

/*+**************************************************************************
 *
 * Module:	High Resolution Timer and Alarm Clock Library 
 *              Routines added by BESSY
 *
 * File:	alm.h
 *
 * Description:	Library package to implement a high resolution timer and alarm clock library.
 *		  mv162                - uses timer 4 on the mvme162's MCC chip (see alm_mcc.c)
 *                mv2400/mv2100/mv5100 - uses timer 4 on the MPIC (see alm_mpic.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:18:43 $
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

#ifndef __ALM_H
#define __ALM_H

#ifdef __cplusplus
extern "C" {
#endif

/*+**************************************************************************
 *	             Alarm Status Information 
 *              (used by global variable alm_status)
 **************************************************************************-*/

#define ALM_STATUS_INIT_DONE    0x1
#define ALM_STATUS_RUNNING      0x2
#define ALM_STATUS_IN_USE       0x4
#define ALM_STATUS_WOKE_UP      0x8
#define ALM_STATUS_ENTRIES      0xff0
#define ALM_STATUS_INT_PRI      0xf000
#define ALM_STATUS_INT_VEC      0xff0000
#define ALM_STATUS_INIT_START   0x1000000
#define ALM_STATUS_CONN_TIMER   0x2000000  
#define ALM_STATUS_MASK         0x03ffffff

typedef struct
{
   unsigned int init_d       : 1;	/* Initializion done flag          */
   unsigned int running      : 1;	/* Counter running flag            */
   unsigned int in_use       : 1;	/* Lists in use flag               */
   unsigned int woke_up      : 1;	/* Int handler was called flag     */
   unsigned int entries      : 8;	/* Used Alarm_Entries              */
   unsigned int int_pri      : 4;       /* interrupt priority level        */
   unsigned int int_vec      : 8;       /* interrupt vector offset         */ 
   unsigned int conn_t       : 1;       /* timer connected to interrupt    */
   unsigned int init_s       : 1;       /* Initialization started flag     */
   unsigned int spare        : 6;       /* make into a complete long word  */  
}alm_status_ts;
      
typedef union
{
    alm_status_ts  _s;
    unsigned long _i;
}alm_status_tu; 

/*+**************************************************************************
 *		Prototypes
 **************************************************************************-*/
      
/* This function sets the timer to cause an interrupt in 
 * the specified # of microseconds 
 */
extern void alm_setup(unsigned long delay);

/* Set the timer counter  */
extern long alm_reset(int enb);

/* Disables the timer interrupt  */
extern void alm_disable(void);    

/* Enable the timer interrupt */
extern long alm_enable(void);

/* Return the alarm clock frequency   */
extern unsigned long alm_freq(void); 

/* Get a time stamp (1 us resolution) */
extern unsigned long alm_get_stamp(void); 

/* Get a time stamp for longer periods. This function
 * makes use of the full 64-bit timebase register
 */
extern double alm_get_stampD(void);

/* Interrupt acknowledge  */
extern void alm_int_ack(void); 

/* Get interrupt delay adjustment */
extern unsigned long alm_int_delay_adjust(void);

/* Get the timer interrupt priority level (in use) */
extern int alm_intLevelGet(void);
  
#ifdef __cplusplus
}
#endif

#endif /* ifndef ALM_H */


