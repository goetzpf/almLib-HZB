/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Debug routines for Alarm Library
 *
 * File:	alm_debug.c
 *
 * Description:	 These routines are used for the alm library debugging.
 *               The functions were mimick the macros such as
 *               DBG_MSG found in the BESSY local support header file
 *               misc/debugmsg.h.  The function alm_set_debug_msg()
 *               is called to set the debug message. Please note that
 *               the definition
 *
 *                   INCLUDE_ALM_DEBUG
 *
 *               must be included in the makefile during compilation
 *               for these routines to be added to the object library,
 *               or kernel, depending on where these files are located.
 *               Please see alm_debug.h for more detail.
 *
 *               The prototypes for these routines are found in alm_debug.h
 *
 * Author(s):	Ralph Lange (used debugmsg.h as reference)
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:22:56 $
 *
 * $Author: Kristi - mimicked DBG_MSG macro from debugmsg.h.
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

#include <logLib.h>                /* for logMsg() prototype */


static int  almDebugLevel = -1;   /* set initial state to no debug messges */


/*+**************************************************************************
 *
 * Function:	alm_set_debug_msg
 *
 * Description:	This function is called to set the debug message level.
 *              A setting of -1 indicates that no debug messages will
 *              be issued.
 *
 * Arg(s) In:  level  - debug level flag (-1=no debug messages issued)
 *
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   None
 *
 **************************************************************************-*/
 
void alm_set_debug_msg(int level)
{ 
   if ( level<-1 ) 
     level = -1;              /* disable error messages */
   else 
     almDebugLevel = level;   /* set  debug level */
   return;
}



/*+**************************************************************************
 *
 * Function:	alm_debug_msg
 *
 * Description:	This function is called to set the debug message level.
 *              If set to to the standard output device depending upon the
 *              current settings of debug level. 
 *
 * Arg(s) In:  level  - debug level flag (-1=no debug messages issued)
 *
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   None
 *
 **************************************************************************-*/
 
void alm_debug_msg(int level,char * string_c,int a, int b)
{
#ifdef INCLUDE_ALM_DEBUG
   if ( level<=almDebugLevel ) 
     logMsg(string_c,a,b,0,0,0,0);
#else
     logMsg("Alarm debug msgs not compiled in. Must define INCLUDE_ALM_DEBUG\n",
            0,0,0,0,0,0);
#endif
   return;
}
