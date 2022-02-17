/*==========================================================
                             alarm
  ==========================================================

Copyright 2022 Helmholtz-Zentrum Berlin für Materialien und Energie GmbH
<https://www.helmholtz-berlin.de>

This file is part of the alarm EPICS support module.

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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include "epicsInterrupt.h"
#include "timer.h"

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

void timer_init(void)
{
}

/* setup counter to go off in <delay> microseconds */
void timer_setup (unsigned long delay)
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
void timer_enable (void)
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
void timer_disable (void)
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
void timer_reset (int enable_it)
{
    if (enable_it) {
        timer_enable();
    } else {
        timer_disable();
    }
}

/* acknowledge an interrupt */
void timer_int_ack (void)
{
}

/* get/set interrupt vector & level */
int timer_install_int_routine (VOID_FUNC_PTR f)
{
    int_handler = f;
    return 0;
}

unsigned timer_get_int_level (void)
{
    return 0;
}

void timer_set_int_level (unsigned level)
{
}

/* return maximum accepted delay for routine 'start' */
unsigned long timer_get_max_delay (void)
{
    return ULONG_MAX;
}

/* get a timestamp in microseconds, as integral or floating point value */
unsigned long timer_get_stamp (void)
{
    struct timespec ts;
    uint64_t us;
    clock_gettime(CLOCKID, &ts);
    us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return (unsigned long)(us & 0xffffffff);
}

double timer_get_stamp_double (void)
{
    struct timespec ts;
    clock_gettime(CLOCKID, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}
