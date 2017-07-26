#ifndef _ALM_MCC_H_
#define _ALM_MCC_H_

/*+*********************************************************************
 *
 * Time-stamp: <01 Jul 04 13:55:49 Kristi Luchini>
 *
 * File:     alm_mcc.h
 * Project:  Alarm Clock Library BSP specific timer chip functions 
 *
 * Descr.:   Header file for the MCChip timer.
 *           This header file relates to the "C" module of the
 *           same name, alm_mcc.c 
 *
 * Author(s):  Kristi Luchini
 *
 * $Revision: 28 $
 * $Date: 2004-07-13 14:39:02 +0200 (Tue, 13 Jul 2004) $
 *
 * $Author: luchini $
 *
 * $Log$
 * Revision 1.1  2004/07/13 12:37:08  luchini
 * low-level timer chip support for VME2 Chip
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
 
/* For tornado 2.0.2 delays (temporary) */
#define INT_INHIBIT_SAFE  50ul
#define INT_SUBTRACT_SAFE  0ul
 
/* 25 MHz CPU */
#define INT_INHIBIT_25    25ul	    /* Minimum interrupt delay           */
#define INT_SUBTRACT_25   46ul	    /* Subtracted from every delay       */

/* 32 MHz CPU */
#define INT_INHIBIT_32    20ul	    /* Minimum interrupt delay           */
#define INT_SUBTRACT_32   34ul	    /* Subtracted from every delay       */
                                    /* Safe values (prelimininary patch) */
/* Magic Words */
#define MAGIC_P  (unsigned long*)(0xfffc1f28)  /* Address of Magic Word  */
#define MAGIC_25 0x32353030ul	               /* ASCII 2500             */
#define MAGIC_32 0x33323030ul	               /* ASCII 3200             */

#define TIMER_INT_LEVEL   1        /* Timer interrupt level              */

#endif /* _ALM_MCC_H_ */
