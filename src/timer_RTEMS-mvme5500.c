#include <limits.h>             /* LONG_MAX                                     */
/* EPICS includes */
#include <epicsInterrupt.h>     /* epicsInterruptLock(), epicsInterruptUnlock() */
#include <epicsTime.h>          /* epicsTimeGetCurrent()                        */
#include <epicsTypes.h>         /* epicsUInt64                                  */
#include <epicsVersion.h>
/* RTEMS includes */
#include <bsp/irq.h>            /* rtems_interrupt_catch()                      */

#include "timer.h"

#include "ppc_timebase_reg.c"

/*+**************************************************************************
 *              Local Definitions
 **************************************************************************-*/

#if EPICS_VERSION==3 && EPICS_REVISION<=14
typedef unsigned long long epicsUInt64;
#endif

unsigned long MV64260_READ32(unsigned long base, unsigned long reg)
{
    return *((unsigned long *) (base + reg));
}

void MV64260_WRITE32_PUSH(unsigned long base, unsigned long reg, unsigned long val)
{
    *((unsigned long *) (reg + base)) = val;
    MV64260_READ32(reg, base);
}

extern unsigned int BSP_bus_frequency; /* make variable visible */

#define MV64260_REG_BASE        0xf1000000 /* Base-Addr of Marvel MV64260 Discovery Chip on the mvme5500 board*/
#define TMR_CNTR_CTRL_0_3       0x864
#define TMR_CNTR3               0x85C
#define TMR_CNTR_INT_MASK_0_3   0x86C
#define TMR_CNTR_INT_CAUSE_0_3  0x868
#define TMR_CNTR3_INT_VEC       9     

#define ULONGLONG_MAX 0xffffffffffffffffull
#define USECS_PER_SEC 1000000ull 


/*+**************************************************************************
 *              Parameter Definitions
 **************************************************************************-*/

#define TMR_CNTR_CTRL_TC3EN_MASK        (1 << 24)
#define TMR_CNTR_CTRL_TC3MOD_MASK       (1 << 25)
#define TMR_CNTR_INT_CAUSE_TC3_MASK     (1 << 3)
#define TMR_CNTR_INT_MASK_TC3_MASK      (1 << 3) 


/*+**************************************************************************
 *
 * Conversion routines
 *
 * These routines convert between timebase resp. timer ticks and microseconds.
 *
 **************************************************************************-*/

/* On MV5500 boards the decrementer & timer use the same 
 * frequency: both were triggered with a quarter of the bus cycle
 */

static unsigned long timer_usec_to_timerticks(unsigned long delay)
{
    epicsUInt64 ticks;
    
/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    ticks = (epicsUInt64) delay * (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
    
    return (unsigned long) (ticks & 0xFFFFFFFF);
}

static unsigned long timer_timerticks_to_usec(unsigned long ticks)
{
/* if the bus frequency is lower than 4Mhz a greater datatype 
 * would be necessary, MV500 has 66Mhz -> so we can ignore this case
 */
    return (epicsUInt64) ticks / (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
}

static unsigned long timer_timebase_to_usec(epicsUInt64 t)
{
    epicsUInt64 delay;

/* during the calculation we need a int64 because the 
 * result is (maybe) larger then the input value 
 * (it takes effect if the bus frequency is above 4Mhz)
 */
    delay = t / (((epicsUInt64) BSP_bus_frequency / 4ULL) / USECS_PER_SEC);
    
    return (unsigned long) (delay & 0xFFFFFFFF);
} 

/*+**************************************************************************
 *
 * API
 *
 **************************************************************************-*/

void timer_init(void)
{
}

unsigned long timer_get_stamp(void)
{
    unsigned long tbu = 0, tbl = 0;

    readTimeBaseReg(&tbu, &tbl);
    return timer_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl);
}

double timer_get_stamp_double(void)
{
    unsigned long tbu = 0, tbl = 0;
    readTimeBaseReg(&tbu, &tbl);
    return (double) timer_timebase_to_usec((epicsUInt64) tbu << 32 | (epicsUInt64) tbl) \
        / (double) USECS_PER_SEC;
}

void timer_setup(unsigned long delay)
{
    unsigned long delay_in_timerticks;
    delay_in_timerticks = timer_usec_to_timerticks(delay);
        
    /* disable counter 3 */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);

    /* set counter mode */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3MOD_MASK); 
    
    /* write value to count from */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR3, delay_in_timerticks);

    /* start counter 3 */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) | TMR_CNTR_CTRL_TC3EN_MASK);
}

void timer_reset(int enable)
{
    register unsigned long zero = 0;

    __asm__ __volatile__(                                    
                         "mttbl %0\n"
	                 "mttbu %0\n"
	                 :
	                 : "r"(zero)
    );

    /*
     * Set the timer basecount register to it's maximum value.
     * and enable or disable counting depending on the input
     * argument <enable>.
     */
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3MOD_MASK); 
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR3, LONG_MAX);

    if (!enable)
    { 
        MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) & ~TMR_CNTR_CTRL_TC3EN_MASK);
    }
    else
    {        
        MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3) | TMR_CNTR_CTRL_TC3EN_MASK);
    }
}

void timer_disable(void)
{
    int lock_stat = epicsInterruptLock();
    
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3) & ~TMR_CNTR_INT_MASK_TC3_MASK); 
	
    epicsInterruptUnlock(lock_stat);
}

void timer_enable(void)
{
    int lock_stat = epicsInterruptLock();
    
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3) | TMR_CNTR_INT_MASK_TC3_MASK); 

    epicsInterruptUnlock(lock_stat);
}

void timer_int_ack(void)
{
    MV64260_WRITE32_PUSH (MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3) & ~TMR_CNTR_INT_CAUSE_TC3_MASK);  
}

static void timer_int_no_op(const rtems_irq_connect_data* irq)
{
    return;
} 

static int timer_int_IsOn(const rtems_irq_connect_data* irq)
{
    return 0;
} 

int timer_install_int_routine(VOID_FUNC_PTR int_handler)
{
    rtems_irq_connect_data irqInfo; 
   
    memset(&irqInfo, 0, sizeof(irqInfo));
    irqInfo.name  = TMR_CNTR3_INT_VEC;
    irqInfo.hdl  = (rtems_irq_hdl) int_handler;
    irqInfo.on   = timer_int_no_op;
    irqInfo.off  = timer_int_no_op;
    irqInfo.isOn = timer_int_IsOn; 

    return BSP_install_rtems_irq_handler( &irqInfo ); 
}

unsigned timer_get_int_level(void)
{
    return TMR_CNTR3_INT_VEC;
}

void timer_set_int_level(unsigned level)
{
/* do nothing, because the mv64260 does not support changing the interrupt level. Function are left for compatability. */
}

unsigned long timer_get_max_delay(void)
{
    static unsigned long alm_max_delay = 0;
    if (!alm_max_delay) {
        alm_max_delay = timer_timerticks_to_usec(LONG_MAX);
    }
    return alm_max_delay;
}

/*+**************************************************************************
 *
 * Test routines
 *
 **************************************************************************-*/

#include <stdio.h>         /* printf */

void timer_dump_registers(void)
{
    printf("*TMR_CNTR3              = *0x%08x = 0x%08lx\n", TMR_CNTR3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR3));
    printf("*TMR_CNTR_CTRL_0_3      = *0x%08x = 0x%08lx\n", TMR_CNTR_CTRL_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_CTRL_0_3));
    printf("*TMR_CNTR_INT_CAUSE_0_3 = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_CAUSE_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_CAUSE_0_3));
    printf("*TMR_CNTR_INT_MASK_0_3  = *0x%08x = 0x%08lx\n", TMR_CNTR_INT_MASK_0_3, MV64260_READ32(MV64260_REG_BASE, TMR_CNTR_INT_MASK_0_3));
}
