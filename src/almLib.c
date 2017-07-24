/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	almLib - High Resolution Timer and Alarm Clock Library
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

#include <devLib.h>
#include <errlog.h>
#include <epicsMutex.h>
#include <epicsInterrupt.h>

#include "timer.h"
#include "almLib.h"

#include <assert.h>


#ifndef min
#define min(a,b) (a)<(b)?(a):(b)
#endif

#ifndef max
#define max(a,b) (a)>(b)?(a):(b)
#endif

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

struct alm_def {
    void            *arg;
    alm_callback    *callback;
    alm_stamp_t     time_due;
    int             active;
    int             enqueued;
    struct alm_def  *next;
};

static alm_init_state_t init_state = ALM_NO_INIT;
                                        /* this module's initialization state */
static epicsMutexId alm_lock;           /* global mutex */
static alm_t first_alm;                 /* head of alm_t object queue */

static void alm_insert(alm_t what);
static void alm_purge(void);
static void alm_remove(alm_t what);
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
    alm_t alm = first_alm;
    alm_stamp_t now;

    timer_int_ack();
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

    if (!from_int_handler) lock_stat = epicsInterruptLock();
    if (from_int_handler || time_due <= next_due) {
        time_now = alm_get_stamp();
        if (time_due < time_now)
            time_due = time_now;
        delay = time_due - time_now;
        max_delay = timer_get_max_delay();
        if (delay > max_delay) delay = max_delay;
        if (delay < MIN_WAIT) delay = MIN_WAIT;
        next_due = time_now + delay;
        timer_setup(delay);
    }
    if (!from_int_handler) epicsInterruptUnlock(lock_stat);
}

/*
 * Get the current timestamp as a 64 bit integer.
 *
 * Note: Must be called at least every ULONG_MAX microseconds, in order
 * to recognize low-level timer count overflow and properly calculate
 * the high word of the returned timestamp. See interrupt handler and
 * alm_init.
 */
alm_stamp_t alm_get_stamp(void)
{
    static alm_stamp_t high_word = 0;
    static unsigned long last_time = 0;
    unsigned long now;
    alm_stamp_t result;
    int lock_key;

    timer_init();
    lock_key = epicsInterruptLock();
    now = timer_get_stamp();
    if (now < last_time) {
        high_word += 0x100000000ull;
    }
    last_time = now;
    result = high_word + now;
    epicsInterruptUnlock(lock_key);
    return result;
}

/*
 * Implementation of high-level interface starts here
 */

void unchecked_alm_start(alm_t what, alm_stamp_t delay)
{
    alm_stamp_t tstart;

    /* to minimize errors, take the timestamp as early as possible */
    tstart = alm_get_stamp();

    epicsMutexMustLock(alm_lock);
    alm_purge();                        /* remove inactive alarms */
    alm_cancel(what);                   /* set alarm to inactive */
    alm_remove(what);                   /* remove it from queue (if enqueued) */
    /* extremely long delays are simply ignored */
    if (delay < MAX_DELAY) {
        what->time_due = tstart + delay;    /* calculate time due */
        what->active = 1;                   /* activate alarm */
        alm_insert(what);                   /* insert it into queue */
        if (what->active)
            alm_setup_alarm(what->time_due, 0); /* setup timer (if necessary) */
    }
    epicsMutexUnlock(alm_lock);
}

/* insert alarm into queue, sorted by time due */
static void alm_insert(alm_t what)
{
    alm_t next = first_alm;
    alm_t prev = 0;

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
    alm_t next = first_alm;

    while (next && !next->active) {
        next->enqueued = 0;
        next = next->next;
    }
    first_alm = next;
}

/* remove alarm from queue, if enqueued */
static void alm_remove(alm_t what)
{
    alm_t next = first_alm;
    alm_t prev = 0;

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

void unchecked_alm_cancel(alm_t alm)
{
    alm->active = 0;
}

alm_t alm_create(alm_callback *callback, void *arg)
{
    alm_t alm = (alm_t) malloc(sizeof(struct alm_def));

    if (!alm) return NULL;
    alm->callback = callback;
    alm->arg = arg;
    alm->time_due = 0;
    alm->active = 0;
    alm->enqueued = 0;
    alm->next = 0;
    return alm;
}

void alm_call_epics_event_signal(void *arg)
{
    epicsEventSignal((epicsEventId)arg);
}

alm_t alm_create_event(epicsEventId ev)
{
    return alm_create(alm_call_epics_event_signal, ev);
}

void unchecked_alm_destroy(alm_t alm)
{
    alm_cancel(alm);
    if (alm->enqueued) {
        assert(init_state == ALM_INIT_OK);
        epicsMutexMustLock(alm_lock);
        alm_remove(alm);
        epicsMutexUnlock(alm_lock);
    }
    assert(!alm->active);
    assert(!alm->enqueued);
    free(alm);
}

alm_init_state_t alm_init_state(void)
{
    return init_state;
}

int alm_init(int intLevel)
{
    int key = epicsInterruptLock();                /* lock interrupts during init */

    if (init_state != ALM_NO_INIT) {
        goto done;
    }
    init_state = ALM_INIT_FAILED;       /* assume init failes */
    timer_init();
    timer_set_int_level(intLevel);
    alm_lock = epicsMutexCreate();
    if (!alm_lock) {
        errlogSevPrintf(errlogFatal,
            "alm_init: semMCreate failed\n");
        goto done;
    }
    if (timer_install_int_routine(alm_int_handler))
    {
        errlogSevPrintf(errlogFatal, "alm_init: devConnectInterrupt failed\n");
	goto done;
    }
    timer_enable();
    alm_setup_alarm(alm_get_stamp() + MAX_WAIT, 0);

    init_state = ALM_INIT_OK;           /* success */

done:
    epicsInterruptUnlock(key);
    return init_state;
}

void alm_dump_alm(alm_t alm)
{
    if (!alm) {
        printf("<NULL>\n");
    } else {
        printf("%p:due="alm_fmt",%s,%s,next=%p\n",
            alm, alm_fmt_arg(alm->time_due),
            alm->active ? "active" : "inactive",
            alm->enqueued ? "enqueued" : "dequeued", alm->next);
    }
}

void alm_dump_queue(void)
{
    alm_t next = first_alm;

    if (init_state != ALM_INIT_OK) {
        printf("not initialized or initialization failed\n");
        return;
    }
    epicsMutexMustLock(alm_lock);
    if (!first_alm) {
        printf("empty\n");
    } else {
        while (next) {
            alm_dump_alm(next);
            next = next->next;
        }
    }
    epicsMutexUnlock(alm_lock);
}

void alm_print_stamp(void)
{
    alm_stamp_t time=alm_get_stamp();
    printf(""alm_fmt"\n",dhi(time),dlo(time));
}

/*
 * Test code follows
 */

#include <semaphore.h>
#include <epicsThread.h>

static long min_error, max_error;

#define ERROR_LIMIT 0x7fffffff

void alm_test_sem(void *parm)
{
    unsigned delay = (unsigned)parm;
    sem_t sem;
    alm_t alm;
    alm_stamp_t t1, t2;
    long real_delay, error;

    sem_init(&sem, 0, 0);
    alm = alm_create_sem(&sem);
    if (!alm) {
        printf("ERROR: alarm creation failed!\n");
        return;
    }
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
    alm_destroy(alm);
    sem_destroy(&sem);
}

void alm_test_sem_many(unsigned delay, unsigned num)
{
    min_error = ERROR_LIMIT;
    max_error = -ERROR_LIMIT;
    while (num) {
        epicsThreadCreate("athx", epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackMedium),
            alm_test_sem, (void*) (delay * num));
        epicsThreadSleep(0.2);
        num--;
    }
    epicsThreadSleep(2.0);
    alm_dump_queue();
    printf("error_range=[%ld..%ld]\n", min_error, max_error);
}

struct testdata {
    alm_stamp_t start, stop, nom_delay;
    alm_t alm;
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
    long min_latency = ERROR_LIMIT, max_latency = -ERROR_LIMIT;

    if (!data) {
        printf("ERROR: memory allocation failed!\n");
        return;
    }
    min_error = ERROR_LIMIT;
    max_error = -ERROR_LIMIT;

    counter = num;
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];
        x->alm = alm_create(test_cb, x);
        x->nom_delay = (((unsigned)rand() << 16) + (unsigned)rand()) % delay;
    }
    printf("init done\n");
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];

        x->start = alm_get_stamp();
        alm_start(x->alm, x->nom_delay);
    }
    while (counter > 0) {
        epicsThreadSleep(1.0/60);
        printf(".");fflush(stdout);
    }
    printf("\n");
    for (n = 0; n < num; n++) {
        struct testdata *x = &data[n];
        alm_stamp_t real_delay = x->stop - x->start;
        long latency = (long long)x->stop - (long long)x->alm->time_due;

        if (!silent) {
            printf("%03u:start="alm_fmt",due="alm_fmt",stop="alm_fmt"\n",
                n, alm_fmt_arg(x->start),
                alm_fmt_arg(x->alm->time_due),
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
        alm_destroy(data[n].alm);
    }
    free(data);
}

void alm_test_create_event(int delay)
{
    alm_delay_t real_delay;
    epicsEventId ev = epicsEventMustCreate(epicsEventEmpty);
    alm_t alm = alm_create_event(ev);
    real_delay = delay > 0 ? delay : 1e6;
    printf("test_alm_create_event start\n");
    alm_start(alm,real_delay);
    epicsEventWait(ev);
    printf("test_alm_create_event stop\n");
    alm_destroy(alm);
    epicsEventDestroy(ev);
}
