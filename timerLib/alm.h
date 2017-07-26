/*+*********************************************************************
 *
 * File:    alm.h
 *
 * Project: High Resolution Timer used by the Alarm Clock Library
 *
 * Descr.:  Interface to Low-level Timer Library
 *
 * Author(s): Kristi Luchini, Ben Franksen, Dan Eichel
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
#ifndef ALM_H
#define ALM_H

#ifdef __cplusplus
extern "C" {
#endif

#define USECS_PER_SEC      1000000ull
/*+**************************************************************************
 *                  Entry table for timer routines
 **************************************************************************-*/

typedef void (*VOID_FUNC_PTR)();

typedef struct {
    /* setup counter to go off in <delay> microseconds */
    void            (*setup) (unsigned long delay);
    /* initialize (reset) counter and enable or disable interrupts */
    void            (*reset) (int enable);
    /* enable or disable interrupts */
    void            (*enable) (void);
    void            (*disable) (void);
    /* acknowledge an interrupt */
    void            (*int_ack) (void);
    /* get/set interrupt vector & level */
    int             (*install_int_routine) (VOID_FUNC_PTR);
    unsigned        (*get_int_level) (void);
    void            (*set_int_level) (unsigned);
    /* return maximum accepted delay for routine 'start' */
    unsigned long   (*get_max_delay) (void);
    /* get a timestamp in microseconds, as integral or floating point value */
    unsigned long   (*get_stamp) (void);
    double          (*get_stamp_double) (void);
} alm_func_tbl_ts;

/*+**************************************************************************
 *                             Prototypes
 **************************************************************************-*/

/*
 * Initialize alarm symbol table with the
 * timer chip functions for the BSP.
 */
extern alm_func_tbl_ts *alm_tbl_init(void);

#ifdef __cplusplus
}
#endif
#endif
