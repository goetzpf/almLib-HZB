/* alm_debug.h */

/*+**************************************************************************
 *
 * Module:	Prootype file for ALM debug messagesY
 *
 * File:	alm_debug.h
 *
 * Description:	Prototype file for alm_debug functions
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:20:34 $
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

#ifndef __ALM_DEBUG_H
#define __ALM_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Set the alarm debug level */
extern void alm_set_debug_level(int level);

/* Issue debug messsage depending on debug level requested as an input
 * argument, compared to that set by the function alm_degug_level().
 * The default setting is that debug messages are disabled.
 */
extern void alm_debug_msg( 
        int    level,           /* debug level (-1 msg disabled) */
	char * string_c,        /* message to be issued to stdio */
	int    a,               /* message string arg #1         */
	int    b                /* message string arg #2         */
	              );
	
/*+**************************************************************************
 *		Macro
 **************************************************************************-*/
 
#ifdef INCLUDE_ALM_DEBUG
#define ALM_DEBUG_MSG(level,string,a,b)   alm_debug_msg(level,string,a,b)
#else
#define ALM_DEBUG_MSG(level,string,a,b)
#endif

#ifdef __cplusplus
}
#endif


#endif /* __ALM_DEBUG_H */

