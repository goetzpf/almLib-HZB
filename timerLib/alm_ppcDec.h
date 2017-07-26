#ifndef _ALM_PPCDEC_H_
#define _ALM_PPCDEC_H_

/*+*********************************************************************
 *
 * Time-stamp: <01 Jul 04 13:55:49 Kristi Luchini>
 *
 * File:     alm_ppcDec.h
 * Project:  Alarm Clock Library BSP specific timer chip functions 
 *
 * Descr.:   Header file for the PowerPC Decrementer Timer.
 *           This header file relates to the "C" module of the
 *           same name, alm_ppcDec.c 
 *
 * Author(s):  Kristi Luchini, Ben Franksen, Dan Eichel
 *
 * $Revision: 58 $
 * $Date: 2006-07-04 10:52:43 +0200 (Tue, 04 Jul 2006) $
 *
 * $Author: franksen $
 *
 * $Log$
 * Revision 1.3  2006/07/04 08:52:43  franksen
 * merged in changes from 3-13 branch
 *
 * Revision 1.2.2.1  2006/06/20 10:28:00  franksen
 * fixed ppc alarm setup routine;
 * ppc get_stamp no longer needs floating point operations;
 * general cleanup, removed useless debug messages + support
 *
 * Revision 1.2  2006/01/17 11:38:35  franksen
 * TIMEBASE_DECREMENTER_PRESCALE is always 4; fixed some typos
 *
 * Revision 1.1  2004/07/13 12:36:42  luchini
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

#ifdef __cplusplus
extern "C" {
#endif


/*+**************************************************************************
 *              Macros Defintions
 **************************************************************************-*/
 
/*
 * Common I/O synchronizing instructions 
 * Force all writes to complete.
 */
#ifndef EIEIO  
#  define EIEIO __asm__ volatile ("eieio")   
#endif
 
 
/*
 * From chapter 1 page 15 (eg page 55) of the Motorola 
 * The time base is a 64-bit register (accessed as two 32-bit registers)
 * that is incremented every four bus clock cycles; external control of the 
 * time base is provided through the time base enable (TBEN) signal.
 * The decrementer is a 32-bit register that generates a decrementer 
 * interrupt exception after a programmable delay. The contents of the
 * decrementer register are decremented once every four bus clock cycles,
 * and the decrementer exception is generated as the count passes through
 * zero.
 *
 * For the motorola mv2400 the timebase register is on the HAWK
 */


/* 
 * timer registers 0-3
 * - current count register
 * - base count register
 * - vector/priority register
 * - destination register
 *
 * In addition to the per-timer registers listed above, there is a single Timer
 * Frequency Register which is used to hold the effective clock frequency of the
 * MPIC timer clock frequency. Start-up code must initialize this 32-bit register
 * to the PCI clock frequency divided by 8.
 *
 * NOTE: Raven MPIC was clocked by the 60x bus clock. Hawk MPIC is clocked
 *       by the PCI bus clock.
 */

#ifdef __cplusplus
}
#endif

#endif /* _ALM_PPCDEC_H_ */
