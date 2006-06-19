/*+**************************************************************************
 *
 * Project:     Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:      Alm - High Resolution Timer and Alarm Clock Library
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

#include <semaphore.h>

/* type of timestamps */
typedef unsigned long long alm_stamp_t;

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
 * This structure should be viewed as opaque. It is exported
 * so that client code can allocate it on the stack.
 * Don't forget to call alm_init if you do this!
 */
struct AlmDesc {
    void            *arg;
    alm_callback    *callback;
    alm_stamp_t     time_due;
    int             active;
    int             enqueued;
    struct AlmDesc  *next;
};
typedef struct AlmDesc *Alm;

/*
 * Must be called once prior to using any of the features of this library.
 * A proper place is from vxWorks startup script, right after loading the code.
 */
extern int alm_init_module(int intLevel);

/*
 * Alm objects can be allocated (alm_create) and freed (alm_destroy)
 * by the library, using teh following routines.
 * Creation includes proper initialization, using the given values.
 */
extern Alm alm_create(alm_callback *callback, void *arg);
extern Alm alm_create_sem(sem_t *sem);
extern void alm_destroy(Alm alm);

/*
 * Explicit initialization and de-initialization for use with
 * stack allocated Alm objects.
 */
extern void alm_init(Alm alm, alm_callback *callback, void *arg);
extern void alm_init_sem(Alm alm, sem_t *sem);
extern void alm_deinit(Alm alm);

/*
 * Start alarm clock for an Alm object. The object's callback will be
 * called (from interrupt handler) afetr <delay> microseconds.
 */
#define MAX_DELAY 0x8000000000000000ull
/*
 * Delays greater than MAX_DELAY (microseconds) are silently 
 * ignored. They would be /a lot/ further into the future than
 * the IOC runs w/o booting (appr. 292471 years).
 */
extern void alm_start(Alm alm, alm_stamp_t delay);

/* Cancel an outstanding alarm */
extern void alm_cancel(Alm alm);

/* Return the current timestamp */
extern alm_stamp_t alm_get_stamp(void);

/* Test routines */
extern void alm_dump_alm(Alm alm);
extern void alm_dump_queue(void);
extern void alm_print_stamp(void);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALMLIB_H */
