/*+*********************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * Description: use timer 2 and 3 of the RTEMS beatnik BSP
 *
 *********************************************************************-*/

#include <stdio.h>
#include <bsp/gt_timer.h>

#include "timer.h"

/* we use two of the 4 timers
 * timer 2 as clock
 * timer 3 as alarm
 */

static uint32_t alarm_timer_no= -1;
static uint32_t clock_timer_no= -1;
static uint32_t alarm_ticks_per_usec=1;

static uint32_t clock_ticks_per_usec=1;
static double clock_ticks_per_sec=1;

static uint32_t alarm_max=0;
static uint32_t alarm_due=0;

static uint32_t clock_h=0;

static void (*callback)(void*)=0;


static void alm_clock_irq(void *dummy)
  {
    clock_h++;
  }

void timer_init(void)
  {
    uint32_t i_alarm_ticks_per_sec, i_clock_ticks_per_sec;
    double d_alarm_ticks_per_usec, d_clock_ticks_per_usec;

    static int done = 0;

    if (!done) {
        done = 1;
    } else {
        return;
    }

    clock_timer_no= 2;
    alarm_timer_no= 3;

    i_alarm_ticks_per_sec= BSP_timer_clock_get(alarm_timer_no);
    d_alarm_ticks_per_usec= i_alarm_ticks_per_sec/1.0E6;
    /* alarm_ticks_per_usec is usually 133.333333 */
    alarm_ticks_per_usec= (unsigned long)d_alarm_ticks_per_usec;
    alarm_max= (unsigned long)(0xffffffff/d_alarm_ticks_per_usec) - 1;

    i_clock_ticks_per_sec= BSP_timer_clock_get(clock_timer_no);
    d_clock_ticks_per_usec= i_clock_ticks_per_sec/1.0E6;
    clock_ticks_per_usec= (unsigned long)d_clock_ticks_per_usec;
    clock_ticks_per_sec= i_clock_ticks_per_sec;

    BSP_timer_setup(clock_timer_no, alm_clock_irq, 0, 1 /*reload*/);
    BSP_timer_start(clock_timer_no, 0xffffffff);
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

static unsigned long long clock_64_bit(void)
  {
    uint32_t h,l;
    do
      {
        h= clock_h;
        l= BSP_timer_read(clock_timer_no);
      } while(h!=clock_h);
    return (((unsigned long long)clock_h) << 32) + (0xffffffffu-l);
  }

unsigned long timer_get_stamp(void)
  {
    return (unsigned long)(clock_64_bit() / clock_ticks_per_usec);
  }

double timer_get_stamp_double(void)
  {
    return clock_64_bit() / clock_ticks_per_sec;
  }

/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

void alm_beatnik_dump(void)
  {
    printf("alarm_timer_no: %d\n", (int)alarm_timer_no);
    printf("clock_timer_no: %d\n", (int)clock_timer_no);
    printf("alarm_ticks_per_usec: %d\n", (unsigned)alarm_ticks_per_usec);
    printf("clock_ticks_per_usec: %d\n", (unsigned)clock_ticks_per_usec);
    printf("clock_ticks_per_sec: %f\n", clock_ticks_per_sec);
    printf("alarm_max: %d\n", (unsigned)alarm_max);
    printf("alarm_due: %d\n", (unsigned)alarm_due);
    printf("clock_h: %d\n", (unsigned)clock_h);
    printf("callback: %p\n", callback);
    printf("timer_get_stamp: %lu\n", timer_get_stamp());
    printf("timer_get_stamp_double: %f\n", timer_get_stamp_double());
  }
