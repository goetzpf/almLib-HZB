/*+**************************************************************************
 *
 * Project:     Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:      almLib - High Resolution Timer and Alarm Clock Library
 *
 * File:        almLib.h
 *
 * Description: Library package to implement a high resolution alarm clock.
 *              Built upon low-level timerLib (see module bspDep).
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
 *                      Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 **************************************************************************-*/

#ifndef ALMLIB_H
#define ALMLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <DbC.h>
#include <epicsEvent.h>

/* type of timestamps (in microseconds) */
typedef unsigned long long alm_stamp_t;
/* type of alarm delays (in microseconds) */
typedef unsigned long long alm_delay_t;

#define dlo(dword) (unsigned long)((dword) % 1000000000llu)
#define dhi(dword) (unsigned long)((dword) / 1000000000llu)
#define alm_fmt_arg(stamp) dhi(stamp),dlo(stamp)
#define alm_fmt "%.0lu%09lu"
/* Usage: printf("timestamp is: "alm_fmt"\n",alm_fmt_arg(timestamp)); */

/*
 * type of callback routines
 *
 * Warning: Callbacks will be called from interrupt context.
 *          Observe all the usual restrictions for interrupt servive code,
 *          i.e. avoid long running operations, never call anything that
 *          might block, etc.
 */
typedef void alm_callback(void*);

/*
 * Type of alarm objects.
 */
typedef struct alm_def *alm_t;

typedef enum {
    ALM_INIT_OK=0,
    ALM_INIT_FAILED=-1,
    ALM_NO_INIT=1
} alm_init_state_t;

/*
 * Must be called once prior to using any of the features of this library,
 * (except for <alm_get_stamp>, which may also be used from interrupt level).
 * A good place is from vxWorks startup script, right after loading the code.
 * Returns ALM_INIT_FAILED on error, ALM_INIT_OK on success.
 * Argument <intLevel> specifies the interrupt level to be used for the
 * interrupt handler taht will call alarm callbacks. Allowed values are
 * BSP specific and thus not specified here.
 */
extern alm_init_state_t alm_init(int intLevel);

/*
 * Return the current init state.
 */
extern alm_init_state_t alm_init_state(void);

/*
 * Create a new alarm. Whenever this alarm expires, <callback> will be
 * called with <arg> as argument. Returns NULL if allocation failes.
 */
extern alm_t alm_create(alm_callback *callback, void *arg);

/*
 * Create a new alarm that gives a semaphore on expiration.
 */
#define alm_create_sem(sem) alm_create((alm_callback*)sem_post, sem);

/*
 * Create a new alarm that gives an epicsEvent on expiration.
 */
extern alm_t alm_create_event(epicsEventId ev);

/*
 * Destroy an alarm object. The alarm object handle that was given as
 * argument must no longer be used.
 */
extern void unchecked_alm_destroy(alm_t alm);

#define alm_destroy(alm)\
    assertPre((alm) != NULL && alm_init_state() == ALM_INIT_OK,\
        alm_destroy(alm))

#define MAX_DELAY 0x8000000000000000ull

/*
 * Start alarm clock for the given alarm object <alm>. The object's callback
 * will be called (from interrupt handler) after <delay> microseconds.
 *
 * Delays greater than MAX_DELAY (microseconds) are silently
 * ignored. They would be /a lot/ further into the future than
 * the IOC runs w/o booting (appr. 292471 years).
 */
extern void unchecked_alm_start(alm_t alm, alm_delay_t delay);

#define alm_start(alm, delay)\
    assertPre((alm) != NULL && alm_init_state() == ALM_INIT_OK,\
        alm_start(alm, delay))

/* Cancel an outstanding alarm */
extern void unchecked_alm_cancel(alm_t alm);

#define alm_cancel(alm)\
    assertPre((alm) != NULL,\
        alm_cancel(alm))

/* Return the current timestamp. This routine may be called from interrupt
   context. */
extern alm_stamp_t alm_get_stamp(void);

/* Test routines */
extern void alm_dump_alm(alm_t alm);
extern void alm_dump_queue(void);
extern void alm_print_stamp(void);
extern void alm_test_cb(unsigned delay, unsigned num, int silent);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALMLIB_H */
