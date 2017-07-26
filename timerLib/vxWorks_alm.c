/*+*********************************************************************
 *
 * File:    alm.c
 *
 * Project: Alarm Clock Timer Library (BSP Specific Code )
 *
 * Descr.:  The purpose of this module is to determine at
 *          run-time which set of  BSP specific timer functions
 *          should be used.  This was done so that the BSP specific
 *          timer code could reside in the EPICS site support
 *          rather than placing the code in the kernel.  However,
 *          the EPICS makefile system builds by architecture in R4.14.X
 *          and in R3.13.X the build for powerPC can also be done by
 *          architecture (ppc604, ppc603,etc). With this in mind the
 *          timer functions for all cpu's supported within an architecture
 *          is included in the build and only at run-time is it determined
 *          which set of these routines (defined in global entry tables)
 *          will be used. For example, if a site uses the mv5100 and the
 *          mv2400, these are both ppc604 CPU's. This applications for EPICS
 *          would then be built using the target architecture set to ppc604
 *          or ppc604_long.  However, we would not know at compile time
 *          which was being used. This information would only be known
 *          at run-time.
 *
 *          Please note, that the user can decide to place this code into
 *          the kernel in
 *                $WIND_BASE/target/src/config/usr/alm_mcc.c
 *                                                 alm_ppcDec.c
 *                $WIND_BASE/target/h/alm.h
 *          And the appropriate "C" modules included in sysLib.c
 *                $WIND_BASE/target/config/mv2400/sysLib.c
 *                    #include "alm_ppcDec.c"
 *
 * Author(s):  Kristi Luchini
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


#include <stdio.h>                      /* for printf prototype     */
#include <string.h>                     /* for strstr() prototype   */
#include <sysLib.h>                     /* for sysModel() prototype */
#include <logLib.h>                     /* for logMsg() prototype   */

#include "alm.h"

/* Include header files with global entry tables for BSP's */
#include "alm_mc68040_mv162.h"
#include "alm_ppc_mv2100.h"
#include "alm_ppc_mv2400-mv5100.h"
#include "alm_ppc_mv5500.h"

/* Structure for bsp name and pointer to timer function table */
typedef struct {
    char *name;                         /* bsp name */
    alm_func_tbl_ts *tbl_ps;            /* function table */
} alm_bsp_tbl_ts;

/*+**************************************************************************
 *
 *  Abs:  Initialize Alarm routine symbol table base
 *        on the bsp
 *
 *  Name: alm_tbl_init
 *
 *  Args: None
 *
 *  Rem:  The purpose of this function is to initialize
 *        the pointer to the alarm symbol table (ie: alm_symTbl_ps)
 *        with the list table of routines for this particular
 *        cpu and chipset. This was done in the EPICS local
 *        support rather than in the bsp for reasons particular
 *        to BESSY.
 *
 *  Side: None
 *
 *  Ret:  alm_func_tbl_ts *
 *
 *           If successful, the pointer to the alarm timer function table,
 *           otherwise, a NULL pointer.
 *
 **************************************************************************-*/

alm_func_tbl_ts *alm_tbl_init(void)
{
    char *model_p = sysModel();         /* name of board */
    static alm_func_tbl_ts *almSymTbl_ps = (void *) 0;    /* result */
    alm_bsp_tbl_ts alm_bsp_as[] = {
#ifdef HAVE_MVME162
        {"MVME162",  alm_mc68040_mv162_tbl_ps},
#endif
#ifdef HAVE_MVME2100
        {"MVME2100", alm_ppc_mv2100_tbl_ps},
#endif
#ifdef HAVE_MVME2400
        {"MVME2432", alm_ppc_mv2400_mv5100_tbl_ps},
#endif
#ifdef HAVE_MVME2700
        {"MVME2700", alm_ppc_mv2400_mv5100_tbl_ps},
#endif
#ifdef HAVE_MVME5100
        {"MVME5100", alm_ppc_mv2400_mv5100_tbl_ps},
#endif
#ifdef HAVE_MVME5500
        {"MVME5500", alm_ppc_mv5500_tbl_ps},
#endif
        {NULL, NULL}
    };
    alm_bsp_tbl_ts *alm_bsp_ps;

    /* Only call this routine once */
    if (almSymTbl_ps) {
        printf("alm_tbl_init: Low-level alarm function table has already been initialized.\n");
    } else {
        /* Search the table */
        for (alm_bsp_ps = alm_bsp_as; alm_bsp_ps->name; alm_bsp_ps++) {
            if (strstr(model_p, alm_bsp_ps->name)) {
                /*
                 * Looks like the timer chip is supported for this cpu.
                 */
                almSymTbl_ps = alm_bsp_ps->tbl_ps;
            }
        }
        if (!almSymTbl_ps) {
            logMsg("alm_tbl_init: Timer chip not supported for the %s\n",
                (int)model_p, 0, 0, 0, 0, 0);
        } 
    }
    return almSymTbl_ps;
}
