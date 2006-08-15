/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	almLib.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
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
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <assert.h>

#include <vxWorks.h>
#include <semLib.h>
#include <intLib.h>

#include <devLib.h>

#include "alm.h"
#include "almLib.h"

/*
 * Main features of this implementation are:
 * - 64 bit timestamps avoid overflow problems
 * - alarm queue is always strictly sorted by time_due
 * - interrupt handler never modifies queue structure
 *   but merely resets alarm's active flag
 * - thus, queue operations can be interrupted at any time
 *   (pointer assignment is assumed to be atomic)
 *   => no int lock necessary during queue operations
 * - only alm_setup_alarm and alm_get_stamp lock interrupts
 */

static SEM_ID alm_lock;                 /* global mutex */
static Alm first_alm;                   /* head of Alm object queue */
static alm_func_tbl_ts *alm_timer;      /* low-level timer routines */

static void alm_insert(Alm what);
static void alm_purge(void);
static void alm_remove(Alm what);
static void alm_setup_alarm(alm_stamp_t time_due, int from_int_handler);

/* Low level stuff */

#define MAX_WAIT 0x80000000ull
#define MIN_WAIT 0x2ull

/*
 * Interrupt handler is called at least every MAX_WAIT microseconds.
 * This ensures that alm_get_stamp is called often enough to check for 
 * timer counter overflow. See alm_get_stamp() below.
 *
 * Note: the interrupt handler does not modify queue structure
 * or its global anchor first_alm. It merely sets active flags to false.
 */
static void alm_int_handler()
{
    Alm alm = first_alm;
    alm_stamp_t now;

    assert(alm_timer);
    alm_timer->int_ack();
    now = alm_get_stamp();
    while (alm && alm->time_due <= now) {
        if (alm->active) {
            alm->callback(alm->arg);
            alm->active = 0;
        }
        alm = alm->next;
    }
    if (alm) {
        alm_setup_alarm(alm->time_due, 1);
    } else {
        alm_setup_alarm(now + MAX_WAIT, 1);
    }
}

/*
 * Make sure an interrupt will be scheduled at time_due.
 *
 * We remember the time stamp when the next interrupt is expected
 * in the static variable next_due. The low-level timer is setup with
 * delay = time_due - time_now, but only if called from interrupt, or else
 * if time_due <= next_due.
 * We also limit the delay to be setup by MAX_WAIT above and by MIN_WAIT below.
 */
static void alm_setup_alarm(alm_stamp_t time_due, int from_int_handler)
{
    alm_stamp_t time_now, delay;
    static alm_stamp_t next_due = 0xffffffffffffffffull;
    int lock_stat = 0;
    unsigned long max_delay;

    if (!from_int_handler) lock_stat = intLock();
    if (from_int_handler || time_due <= next_due) {
        time_now = alm_get_stamp();
        if (time_due < time_now)
            time_due = time_now;
        delay = time_due - time_now;
        max_delay = alm_timer->get_max_delay();
        if (delay > max_delay) delay = max_delay;
        if (delay < MIN_WAIT) delay = MIN_WAIT;
        next_due = time_now + delay;
        alm_timer->setup(delay);
    }
    if (!from_int_handler) intUnlock(lock_stat);
}

/*
 * Get the current timestamp as a 64 bit integer.
 *
 * Note: Must be called at least every ULONG_MAX microseconds, in order
 * to recognize low-level timer count overflow and properly calculate
 * the high word of the returned timestamp. See interrupt handler and
 * alm_init_module.
 */
alm_stamp_t alm_get_stamp(void)
{
    static alm_stamp_t high_word = 0;
    static unsigned long last_time = 0;
    unsigned long now;
    alm_stamp_t result;
    int lock_key;

    lock_key = intLock();
    now = alm_timer->get_stamp();
    if (now < last_time) {
        high_word += 0x100000000ull;
    }
    last_time = now;
    result = high_word + now;
    intUnlock(lock_key);
    return result;
}

/*
 * Implementation of high-level interface starts here
 */

void alm_start(Alm what, alm_stamp_t delay)
{
    alm_stamp_t tstart;

    assert(alm_timer);
    /* to minimize errors, take the timestamp as early as possible */
    tstart = alm_get_stamp();

    assert(alm_lock);
    assert(what);
    /* extremely long delays are simply ignored */
    if (delay > MAX_DELAY) {
        return;
    }
    semTake(alm_lock, WAIT_FOREVER);
    alm_purge();                        /* remove inactive alarms */
    alm_cancel(what);                   /* set alarm to inactive */
    alm_remove(what);                   /* remove it from queue (if enqueued) */
    what->time_due = tstart + delay;    /* calculate time due */
    what->active = 1;                   /* activate alarm */
    alm_insert(what);                   /* insert it into queue */
    if (what->active)
        alm_setup_alarm(what->time_due, 0); /* setup timer (if necessary) */
    semGive(alm_lock);
}

/* insert alarm into queue, sorted by time due */
static void alm_insert(Alm what)
{
    Alm next = first_alm;
    Alm prev = 0;

    assert(!what->enqueued);
    while (next && next->time_due <= what->time_due) {
        prev = next;
        next = next->next;
    }
    what->next = next;
    what->enqueued = 1;
    if (!prev) {
        first_alm = what;
    } else {
        prev->next = what;
    }
}

/* remove inactive alarms from head of queue */
static void alm_purge()
{
    Alm next = first_alm;

    while (next && !next->active) {
        next->enqueued = 0;
        next = next->next;
    }
    first_alm = next;
}

/* remove alarm from queue, if enqueued */
static void alm_remove(Alm what)
{
    Alm next = first_alm;
    Alm prev = 0;

    assert(what);
    if (!what->enqueued) {
        return;
    }
    if (!first_alm) {
        return;
    }
    while (next && next != what) {
        prev = next;
        next = next->next;
    }
    if (!prev) {                        /* found and first element in queue */
        first_alm = what->next;
    } else if (next) {                  /* found */
        prev->next = what->next;
    }
    what->enqueued = 0;
}

void alm_cancel(Alm what)
{
    assert(what);
    what->active = 0;
}

void alm_init(Alm alm, alm_callback *callback, void *arg)
{
    assert(alm);
    alm->callback = callback;
    alm->arg = arg;
    alm->time_due = 0;
    alm->active = 0;
    alm->enqueued = 0;
    alm->next = 0;
}

void alm_init_sem(Alm alm, sem_t * sem)
{
    alm_init(alm, (alm_callback*)sem_post, sem);
}

void alm_deinit(Alm alm)
{
    assert(alm_timer);
    assert(alm_lock);
    assert(alm);

    alm_cancel(alm);
    if (!alm->enqueued) {
        return;
    }
    semTake(alm_lock, WAIT_FOREVER);
    alm_remove(alm);
    semGive(alm_lock);

    assert(!alm->active);
    assert(!alm->enqueued);
}

Alm alm_create(alm_callback *callback, void *arg)
{
    Alm alm = (Alm) malloc(sizeof(struct AlmDesc));

    alm_init(alm, callback, arg);
    return alm;
}

Alm alm_create_sem(sem_t * sem)
{
    Alm alm = (Alm) malloc(sizeof(struct AlmDesc));

    alm_init_sem(alm, sem);
    return alm;
}

void alm_destroy(Alm alm)
{
    alm_deinit(alm);
    free(alm);
}

int alm_init_module(int intLevel)
{
    if (!alm_timer) {
        alm_timer = alm_tbl_init();
        if (!alm_timer) {
            errlogSevPrintf(errlogFatal,
                "alm_init_module: alm_init_symTbl failed\n");
            return -1;
        }
        if (intLevel < 0 || intLevel > 7) {
            errlogSevPrintf(errlogInfo,
                "alm_init_module: invalid intLevel, using default\n");
        } else {
            alm_timer->set_int_level(intLevel);
        }
        alm_lock = semMCreate(
            SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
        if (!alm_lock) {
            errlogSevPrintf(errlogFatal,
                "alm_init_module: semMCreate failed\n");
            return -1;
        }
        if (devConnectInterrupt(intCPU, alm_timer->get_int_vector(),
                alm_int_handler, 0)) {
            errlogSevPrintf(errlogFatal,
                "alm_init_module: devConnectInterrupt failed\n");
            return -1;
        }
        alm_timer->enable();
        alm_setup_alarm(alm_get_stamp() + MAX_WAIT, 0);
    }
    return 0;
}

void alm_dump_alm(Alm alm)
{
    assert(alm);
    printf("%p:due="alm_fmt",%s,%s,next=%p\n",
        alm, dhi(alm->time_due), dlo(alm->time_due),
        alm->active ? "active" : "inactive",
        alm->enqueued ? "enqueued" : "dequeued", alm->next);
}

void alm_dump_queue(void)
{
    Alm next = first_alm;

    assert(alm_lock);
    semTake(alm_lock, WAIT_FOREVER);
    if (!first_alm) {
        printf("empty\n");
    } else {
        while (next) {
            alm_dump_alm(next);
            next = next->next;
        }
    }
    semGive(alm_lock);
}

void alm_print_stamp(void)
{
    alm_stamp_t time=alm_get_stamp();
    printf(""alm_fmt"\n",dhi(time),dlo(time));
}

/*
 * Test code follows
 */

#include <taskLib.h>
#include <sysLib.h>
#include <usrLib.h>

static long min_error, max_error;

void alm_test_sem(unsigned delay)
{
    sem_t sem;
    struct AlmDesc almd;
    Alm alm = &almd;
    alm_stamp_t t1, t2;
    long real_delay, error;

    sem_init(&sem, 0, 0);
    alm_init_sem(alm, &sem);
    t1 = alm_get_stamp();
    alm_start(alm, delay);
    printf("alm_start with delay=%u (time="alm_fmt")\n", 
        delay, dhi(t1), dlo(t1));
    sem_wait(&sem);
    t2 = alm_get_stamp();
    real_delay = t2 - t1;
    error = (long long)real_delay - (long long)delay;
    printf("done (time="alm_fmt", real_delay="alm_fmt", error=%ld)\n",
        dhi(t2), dlo(t2), dhi(real_delay), dlo(real_delay), error);
    min_error = min(min_error, error);
    max_error = max(max_error, error);
    alm_deinit(alm);
    sem_destroy(&sem);
}

void alm_test_sem_many(unsigned delay, unsigned num)
{
    min_error = LONG_MAX;
    max_error = LONG_MIN;
    while (num) {
        sp((FUNCPTR)alm_test_sem, (unsigned)(delay * num),0,0,0,0,0,0,0,0);
        taskDelay(sysClkRateGet()/5);
        num--;
    }
    taskDelay(sysClkRateGet()*2);
    alm_dump_queue();
    printf("error_range=[%ld..%ld]\n", min_error, max_error);
}

struct testdata {
    alm_stamp_t start, stop, nom_delay;
    struct AlmDesc almd;
};

static int counter;

void test_cb(void *arg)
{
    struct testdata *x = (struct testdata *)arg;

    x->stop = alm_get_stamp();
    counter--;
}

void alm_test_cb(unsigned delay, unsigned num, int silent)
{
    unsigned n = num;
    struct testdata *data = calloc(num, sizeof(struct testdata));
    long min_latency = LONG_MAX, max_latency = LONG_MIN;

    assert(data);
    min_error = LONG_MAX;
    max_error = LONG_MIN;

    counter = num;
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];
        Alm alm = &x->almd;

        alm_init(alm, test_cb, x);
        x->nom_delay = (((unsigned)rand() << 16) + (unsigned)rand()) % delay;
    }
    printf("init done\n");
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];
        Alm alm = &x->almd;

        x->start = alm_get_stamp();
        alm_start(alm, x->nom_delay);
    }
    while (counter > 0) {
        taskDelay(1);
        printf(".");fflush(stdout);
    }
    printf("\n");
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];
        alm_stamp_t real_delay = x->stop - x->start;
        long latency = (long long)x->stop - (long long)x->almd.time_due;

        if (!silent) {
            printf("%03u:start="alm_fmt",due="alm_fmt",stop="alm_fmt"\n",
                n, alm_fmt_arg(x->start),
                alm_fmt_arg(x->almd.time_due),
                alm_fmt_arg(x->stop));
            printf("%03u:n_delay="alm_fmt",r_delay="alm_fmt",latency=%ld\n",
                n, alm_fmt_arg(x->nom_delay), alm_fmt_arg(real_delay), latency);
        }
        min_latency = min(min_latency, latency);
        max_latency = max(max_latency, latency);
    }
    printf("latency_range=[%ld..%ld]\n", min_latency, max_latency);
    if (!silent) {
        alm_dump_queue();
    }
    for (n = 0; n < num; n++) {
        alm_deinit(&data[n].almd);
    }
    free(data);
}
