/*+*********************************************************************
 *
 * Time-stamp: <>
 *
 * File:       alm_ppc_mv2400-mv5500.c
 * Project:    Alarm High Resolution Timer and Alarm Clock Library
 *             (BSP Specific)
 *
 * Descr.:   This module includes the "C" file and header files
 *           for the PowerPC decrentor timer chip on the mv2400, mv2700 & mv5100
 *           processor board.  
 *
 * Author(s):  Kristi Luchini, Ben Franksen, Dan Eichel
 *
 * Revision 1.2 2009/03/26 9:47:15  eichel
 * uses os-independent variants (EPICS) for datatypes, semaphores, interrupts and system-timer
 * 
 * Revision 1.1.2.1  2006/06/30 08:30:30  franksen
 * almost complete redesign; many fixes and simplifications
 *
 * Revision 1.1  2004/07/13 12:36:43  luchini
 * low-level timer chip support for powerPC
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
 * Copyright (c) 1997, 2009 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/

/* Address definition only for vxWorks targets necessary, under RTEMS we can retrieve 
   the base addr of generic PIC-Driver.                                               */
#define BASE           0xfc000000
#define TIMER3_INT_VEC 0x23

#include "alm_ppcDec.c"        /* Generic PPC decrementer timer code */

/* Global pointer to the mv2400 timer function entry table. */
alm_func_tbl_ts *alm_ppc_mv2400_mv5100_tbl_ps = &alm_ppcDecTbl_s;
