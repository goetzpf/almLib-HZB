#ifndef TIMER_H
#define TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#define USECS_PER_SEC      1000000ull

typedef void (*VOID_FUNC_PTR)();

/* initialization */
void timer_init(void);
/* setup counter to go off in <delay> microseconds */
void timer_setup(unsigned long delay);
/* initialize (reset) counter and enable or disable interrupts */
void timer_reset(int enable);
/* enable or disable interrupts */
void timer_enable(void);
void timer_disable(void);
/* acknowledge an interrupt */
void timer_int_ack(void);
/* get/set interrupt vector & level */
int timer_install_int_routine(VOID_FUNC_PTR);
unsigned timer_get_int_level(void);
void timer_set_int_level(unsigned);
/* return maximum accepted delay for routine 'start' */
unsigned long timer_get_max_delay(void);
/* get a timestamp in microseconds, as integral or floating point value */
unsigned long timer_get_stamp(void);
double timer_get_stamp_double(void);

#ifdef __cplusplus
}
#endif
#endif
