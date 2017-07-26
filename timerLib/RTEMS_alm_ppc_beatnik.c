/*+*********************************************************************
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * Description: use timer 2 and 3 of the timer support of RTEMS beatnik BSP for
 *              BSPDEP_TIMER.
 *              
 *
 *********************************************************************-*/

#include <stdio.h>

#include <alm.h>
#include <bsp/gt_timer.h>

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

void alm_lowlevel_init(void)
  {
    uint32_t i_alarm_ticks_per_sec, i_clock_ticks_per_sec;
    double d_alarm_ticks_per_usec, d_clock_ticks_per_usec;

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

/*+**************************************************************************
 *
 * Function:    ALM_setup
 *
 * Description: Set up the counter to go off in <delay> microseconds.
 *
 * Arg(s) In:   delay  -  Demanded timer delay in microseconds.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_setup(unsigned long delay)
  {
    alarm_due= delay * alarm_ticks_per_usec;
    BSP_timer_start(alarm_timer_no, alarm_due);
  }

/*+**************************************************************************
 *
 * Function:	ALM_reset
 *
 * Description:	Set the timer to its max value and dis- or enable counting.
 *
 * Arg(s) In:	enable  - should counting be enabled?
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_reset(int enable)
  {
    BSP_timer_stop(alarm_timer_no);
    if (enable)
      { 
        alarm_due= 0xffffffff;
        BSP_timer_start(alarm_timer_no, alarm_due);
      }
  }

/*+**************************************************************************
 *
 * Function:    ALM_enable
 *
 * Description: Enable interrupts for this timer.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_enable(void)
  { 
    BSP_timer_setup(alarm_timer_no, callback, 0, 0 /* reload=0 */);
  }

/*+**************************************************************************
 *
 * Function:    ALM_disable
 *
 * Description: Disable interrupts for this timer.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_disable(void)
  { 
    BSP_timer_setup(alarm_timer_no, 0, 0, 0 /* reload=0 */);
  }

/*+**************************************************************************
 *
 * Function:    ALM_int_ack
 *
 * Description: Disable the counter, so that no more interrupts occur.
 *              If this is not called as the first action in the interrupt
 *              handler, a vxWorks workQueue overflow panic might result.
 *              This is because the ppcDec timers automatically reload
 *              themselves with the last value.
 *
 *              Note: this has nothing to do with the actual interrupt
 *              acknowledge, which is done by vxWorks.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

static void ALM_int_ack(void)
  { ; }

/*+**************************************************************************
 * 
 * Function:    ALM_install_int_routine
 *
 * Description: This routine connects the interupt vector table offset
 *              used for timer #3 with the routine to be called.
 *
 * Arg(s) In:   Interrupt handler.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   OK or ERROR.
 *
 **************************************************************************-*/ 

static int ALM_install_int_routine(void (*int_handler)(void*))
  {
    callback= int_handler;
    return 0;
  }

/*+**************************************************************************
 * 
 * Function:    ALM_get_int_level
 *
 * Description: Return the currently set interrupt priority level.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt priority level (0-15).
 *
 **************************************************************************-*/ 

static unsigned ALM_get_int_level(void)
  { return 0; }

/*+**************************************************************************
 * 
 * Function:    ALM_set_int_level
 *
 * Description: Set the interupt level for timer #3.
 *
 * Arg(s) In:   Interrupt level.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/ 

static void ALM_set_int_level(unsigned level)
  { ; }

/*+**************************************************************************
 *
 * Function:    ALM_get_max_delay
 *
 * Description: Return the maxmimum possible delay (in microseconds)
 *              that is accepted as argument to ALM_setup.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Maxmimum possible delay.
 *
 **************************************************************************-*/

static unsigned long ALM_get_max_delay(void)
  {
    return alarm_max;
  }

/*+**************************************************************************
 *
 * Function:	ALM_get_stamp
 *
 * Description: Return the current timestamp, converted to microsecond
 *              units, as an unsigned 32 bit integral value.
 *              Note: We have to make sure that the returned timestamp
 *              will correctly overflow after ULONG_MAX microseconds.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in microseconds
 *
 **************************************************************************-*/

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

static unsigned long ALM_get_stamp(void)
  {
    return (unsigned long)(clock_64_bit() / clock_ticks_per_usec);
  }

/*+**************************************************************************
 *
 * Function:	ALM_get_stamp_double
 *
 * Description: This function is used for measuring longer times 
 *              that require the full 64-bit resolution. The value is 
 *              returned in seconds.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in seconds
 *
 **************************************************************************-*/

static double ALM_get_stamp_double(void)
  {
    return clock_64_bit() / clock_ticks_per_sec;
  }

/*+**************************************************************************
 *
 * Function:	alm_beatnik_dump
 *
 * Description: Dump some internal variables
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None
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
    printf("ALM_get_stamp: %lu\n", ALM_get_stamp());
    printf("ALM_get_stamp_double: %f\n", ALM_get_stamp_double());
  }

/*+**************************************************************************
 *
 * Timer function entry table
 *
 **************************************************************************-*/

alm_func_tbl_ts alm_beatnik_Tbl_s = {
    ALM_setup,
    ALM_reset,
    ALM_enable,
    ALM_disable,
    ALM_int_ack,
    ALM_install_int_routine,
    ALM_get_int_level,
    ALM_set_int_level,
    ALM_get_max_delay,
    ALM_get_stamp,
    ALM_get_stamp_double
};

alm_func_tbl_ts *alm_ppc_beatnik_tbl_ps = &alm_beatnik_Tbl_s;

