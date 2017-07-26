#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include "epicsInterrupt.h"
#include "alm.h"

#define CLOCKID CLOCK_REALTIME

static timer_t timerid;
static VOID_FUNC_PTR int_handler;

#define errExit(msg) do { \
    perror(msg); return; \
} while (0)

static void handler(union sigval sig)
{
    int lock_stat = epicsInterruptLock();
    int_handler();
    epicsInterruptUnlock(lock_stat);
}

/* setup counter to go off in <delay> microseconds */
static void setup (unsigned long delay)
{
    struct itimerspec its;

    its.it_value.tv_sec = delay / 1000000;
    its.it_value.tv_nsec = delay % 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) < 0)
        errExit("timer_settime");

}

/* enable interrupts */
static void enable (void)
{
    struct sigevent sev;

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = handler;
    sev.sigev_notify_attributes = 0;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCKID, &sev, &timerid) < 0)
        errExit("timer_create");
}

/* disable interrupts */
static void disable (void)
{
    struct itimerspec its;

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) < 0)
        errExit("timer_settime");
}

/* initialize (reset) counter and enable or disable interrupts */
static void reset (int enable_it)
{
    if (enable_it) {
        enable();
    } else {
        disable();
    }
}

/* acknowledge an interrupt */
static void int_ack (void)
{
}

/* get/set interrupt vector & level */
static int install_int_routine (VOID_FUNC_PTR f)
{
    int_handler = f;
    return 0;
}

static unsigned get_int_level (void)
{
    return 0;
}

static void set_int_level (unsigned level)
{
}

/* return maximum accepted delay for routine 'start' */
static unsigned long get_max_delay (void)
{
    return ULONG_MAX;
}

/* get a timestamp in microseconds, as integral or floating point value */
static unsigned long get_stamp (void)
{
    struct timespec ts;
    uint64_t us;
    clock_gettime(CLOCKID, &ts);
    us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return (unsigned long)(us & 0xffffffff);
}

static double get_stamp_double (void)
{
    struct timespec ts;
    clock_gettime(CLOCKID, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}

alm_func_tbl_ts *alm_tbl_init(void)
{
    alm_func_tbl_ts *tbl = calloc(1,sizeof(alm_func_tbl_ts));

    if (!tbl) {
        printf("alm_tbl_init: out of memory\n");
    } else {
        tbl->setup = setup;
        tbl->reset = reset;
        tbl->enable = enable;
        tbl->disable = disable;
        tbl->int_ack = int_ack;
        tbl->install_int_routine = install_int_routine;
        tbl->get_int_level = get_int_level;
        tbl->set_int_level = set_int_level;
        tbl->get_max_delay = get_max_delay;
        tbl->get_stamp = get_stamp;
        tbl->get_stamp_double = get_stamp_double;
    }
    return tbl;
}
