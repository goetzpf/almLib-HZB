/*+*********************************************************************
 *
 * Time-stamp: <04 Jun 04 10:22:13 Kristi Luchini>
 *
 * File:       alm_mcc.h
 * Project:    
 *
 * Descr.:  Parameter definitions for the MCC Timer on the
 *          VME2 chip used by the alarm support software.
 *
 * Author(s):  Ralph Lange
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:19:33 $
 *
 * $Author: luchini $
 *
 * $Log: alm_mcc.h,v $
 * Revision 2.1  2004/06/22 09:19:33  luchini
 * prototype file for mv162 alm routines
 *
 *
 * This software is copyrighted by the BERLINER SPEICHERRING 
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY. 
 * The following terms apply to all files associated with the 
 * software. 
 *
 * BESSY hereby grants permission to use, copy, and modify this 
 * software and its documentation for non-commercial educational or 
 * research purposes, provided that existing copyright notices are 
 * retained in all copies. 
 *
 * The receiver of the software provides BESSY with all enhancements, 
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, 
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING 
 * OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY 
 * DERIVATIVES THEREOF, EVEN IF BESSY HAS BEEN ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR 
 * A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS 
 * PROVIDED ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO 
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR 
 * MODIFICATIONS. 
 *
 * Copyright (c) 1997 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/
#ifndef __ALM_MCC_H_
#define __ALM_MCC_H_

/* 
 * Please note that you must define MCC_BASE_ADRS before
 * the mcchip.h header file is include.
 */
#include <mv162.h>            /* for MCC_BASE_ADRS & MCC_INT_VEC_BASE */
#include <drv/multi/mcchip.h>  
			
/* For tornado 2.0.2 delays (temporary) */
#define INT_INHIBIT_SAFE  50ul
#define INT_SUBTRACT_SAFE  0ul
 
/* 25 MHz CPU */
#define INT_INHIBIT_25    25ul	/* Minimum interrupt delay */
#define INT_SUBTRACT_25   46ul	/* Subtracted from every delay */

/* 32 MHz CPU */
#define INT_INHIBIT_32    20ul	/* Minimum interrupt delay */
#define INT_SUBTRACT_32   34ul	/* Subtracted from every delay */
                                /* Safe values (prelimininary patch) */
/* Magic Words */
#define MAGIC_P  (unsigned long*)(0xfffc1f28)	/* Address of Magic Word */
#define MAGIC_25 0x32353030ul	/* ASCII 2500 */
#define MAGIC_32 0x33323030ul	/* ASCII 3200 */

/* Timer #3 interrupt vector offset number */
#define TIMER4_INT_VEC  (MCC_INT_VEC_BASE + MCC_INT_TT4)

#endif /* ALM_MCC_H */
