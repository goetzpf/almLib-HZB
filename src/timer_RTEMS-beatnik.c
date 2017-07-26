/*+*********************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * Description: use timer 3 via RTEMS beatnik BSP
 *
 *********************************************************************-*/

#include <stdio.h>
#include <bsp/gt_timer.h>

#include "timer.h"

#include "ppc_timebase_reg.c"

static uint32_t alarm_timer_no= -1;
static uint32_t alarm_ticks_per_usec=1;

static uint32_t alarm_max=0;
static uint32_t alarm_due=0;

static void (*callback)(void*)=0;

void timer_init(void)
  {
    uint32_t i_alarm_ticks_per_sec;
    double d_alarm_ticks_per_usec;

    static int done = 0;

    if (!done) {
        done = 1;
    } else {
        return;
    }

    alarm_timer_no= 3;

    i_alarm_ticks_per_sec= BSP_timer_clock_get(alarm_timer_no);
    d_alarm_ticks_per_usec= i_alarm_ticks_per_sec/1.0E6;
    /* alarm_ticks_per_usec is usually 133.333333 */
    alarm_ticks_per_usec= (unsigned long)d_alarm_ticks_per_usec;
    alarm_max= (unsigned long)(0xffffffff/d_alarm_ticks_per_usec) - 1;
  }

void timer_setup(unsigned long delay)
  {
    alarm_due= delay * alarm_ticks_per_usec;
    BSP_timer_start(alarm_timer_no, alarm_due);
  }

void timer_reset(int enable)
  {
    BSP_timer_stop(alarm_timer_no);
    if (enable)
      { 
        alarm_due= 0xffffffff;
        BSP_timer_start(alarm_timer_no, alarm_due);
      }
  }

void timer_enable(void)
  { 
    BSP_timer_setup(alarm_timer_no, callback, 0, 0 /* reload=0 */);
  }

void timer_disable(void)
  { 
    BSP_timer_setup(alarm_timer_no, 0, 0, 0 /* reload=0 */);
  }

void timer_int_ack(void)
  { ; }

int timer_install_int_routine(void (*int_handler)(void*))
  {
    callback= int_handler;
    return 0;
  }

unsigned timer_get_int_level(void)
  { return 0; }

void timer_set_int_level(unsigned level)
  { ; }

unsigned long timer_get_max_delay(void)
  {
    return alarm_max;
  }

extern unsigned int BSP_bus_frequency; /* make variable visible */

static unsigned long timer_timebase_to_usec(uint64_t t)
{
    uint64_t delay;

    /* during the calculation we need a int64 because the 
     * result is (maybe) larger then the input value 
     * (it takes effect if the bus frequency is above 4Mhz)
     */
    delay = t / (((uint64_t) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);

    return (unsigned long) (delay & 0xFFFFFFFF);
} 

unsigned long timer_get_stamp(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return timer_timebase_to_usec((uint64_t) tbu << 32 | (uint64_t) tbl);
}

double timer_get_stamp_double(void)
{
    unsigned long tbu = 0, tbl = 0;
    readTimeBaseReg(&tbu, &tbl);
    return (double)timer_timebase_to_usec((uint64_t) tbu << 32 |
        (uint64_t) tbl)
        / (double)USECS_PER_SEC;
}

/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

void alm_beatnik_dump(void)
  {
    printf("alarm_timer_no: %d\n", (int)alarm_timer_no);
    printf("alarm_ticks_per_usec: %d\n", (unsigned)alarm_ticks_per_usec);
    printf("alarm_max: %d\n", (unsigned)alarm_max);
    printf("alarm_due: %d\n", (unsigned)alarm_due);
    printf("callback: %p\n", callback);
    printf("timer_get_stamp: %lu\n", timer_get_stamp());
    printf("timer_get_stamp_double: %f\n", timer_get_stamp_double());
  }
