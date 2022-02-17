/*==========================================================
                             alarm
  ==========================================================

Copyright 2022 Helmholtz-Zentrum Berlin für Materialien und Energie GmbH
<https://www.helmholtz-berlin.de>

This file is part of the alarm EPICS support module.

Module     : almLib - High Resolution Timer and Alarm Clock Library
Description: Library package to implement a high resolution alarm clock.
             Built upon low-level timerLib (see module bspDep).

alarm is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

alarm is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with alarm.  If not, see <https://www.gnu.org/licenses/>.
*/

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
extern void alm_test_cb(unsigned delay, unsigned num, int overlap, int verbose);
extern void alm_test_create_event(int delay);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALMLIB_H */
