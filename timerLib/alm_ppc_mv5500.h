#ifndef _ALM_PPC_MV5500_H_
#define _ALM_PPC_MV5500_H_

/*+*********************************************************************
 *
 * Time-stamp: <01 Jul 04 13:55:49 Kristi Luchini>
 *
 * File:     alm_ppc_mv5500.h
 * Project:  Alarm Clock Library BSP specific timer chip functions 
 *
 * Descr.:   External globals for the PowerPC Decrimenter timer.   
 *           This header file relates to the "C" module of the
 *           same name, alm_ppc_mv5500.c 
 *
 * Author(s):  Ben Franksen
 *             Dan Eichel
 *
 * Revision 1.1  2008/11/26 09:15:00  eichel
 * low-level timer chip support for powerPC mv5500 Boards
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
 * Copyright (c) 2008, 2009 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/

#ifdef __cplusplus
extern "C" {
#endif
    
#include "alm.h"                /* for alm_func_tbl_ts */

extern alm_func_tbl_ts *alm_ppc_mv5500_tbl_ps;

#ifdef __cplusplus
}
#endif
#endif /* _ALM_PPC_MV5500_H_ */
