/*+*********************************************************************
 *
 * Time-stamp: <04 Jun 04 11:26:15 Kristi Luchini>
 *
 * File:       timer_ppcDec.h
 * Project:    
 *
 * Descr.:     This is the prototype file for
 *             external functions found in
 *             the module timer_ppcDec.c  
 *
 * Author(s):  Kristi Luchini
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:21:28 $
 *
 * $Author: luchini $
 *
 * $Log: timer_ppcDec.h,v $
 * Revision 2.1  2004/06/22 09:21:28  luchini
 * prototype file for powerPc timer chip test routines
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
 
#ifndef _TIMER_PPCDEC_H_
#define _TIMER_PPCDEC_H_

long          timer_init(unsigned long delay);
void          timer_isr(void);
void          timer_report(void);
UINT32        timer_delay_usec(unsigned long delay);
UINT32        timer_delay_msec(unsigned long delay);   
long          timer_start(void);
long          timer_stop(void);
long          timer_int_enable(void);
long          timer_int_disable(void);
unsigned long timer_rd_ccnt(void);
unsigned long timer_rd_bcnt(void);
unsigned long timer_rd_dest(void);
unsigned long timer_rd_freq(void);
long          timer_set_delay(unsigned long delay);
long          timer_set_freq(void);
long          timer_set_bcnt(unsigned long val,int enb);
double        readTimebase(void);
void          benchmark(int nticks);
void          benchmark_double(int nticks);

#endif /* __TIMER_PPCDEC_H */
