#ifndef _ALM_MC68040_MV162_H_
#define _ALM_MC68040_MV162_H_

/*+*********************************************************************
 *
 * Time-stamp: <01 Jul 04 13:55:49 Kristi Luchini>
 *
 * File:     alm_mc68040_mv162.h
 * Project:  Alarm Clock Library BSP specific timer chip functions 
 *
 * Descr.:   External globals for the MV162 MCChip timer.   
 *           This header file relates to the "C" module of the
 *           same name, alm_mc68040_mv162.c 
 *
 * Author(s):  Kristi Luchini
 *
 * Revision 1.1  2004/07/13 12:39:02  luchini
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
 
#ifdef __cplusplus
extern "C" {
#endif

#include "alm.h"                        /* for alm_func_tbl_ts */

extern alm_func_tbl_ts *alm_mc68040_mv162_tbl_ps;

#ifdef __cplusplus
}
#endif
#endif /* _ALM_MC68040_MV162_H_ */
