static char RcsId[]      =
"@(#)$Id: alm_ppcDec.c,v 2.2 2004/06/24 11:23:48 luchini Exp $";

/*+*********************************************************************
 *
 * Time-stamp: <04 Jun 04 11:54:51 Kristi Luchini>
 *
 * File:       alm_ppcDec.c
 * Project:    
 *
 * Descr.:     PowerPC decrementer timer #3 used to set interrupt
 *             user after a specified number of microseconds.
 *
 *             The software for the timer chip found on the mv162 
 *             is in the file alm_mcc.c
 *
 *             Debug messages are used in the macro ALM_DEBUG_MSG()
 *             This macro is defined to be empty if the definition
 *                    INCLUDE_ALM_DEBUG
 *             has not been included prior to alm_debug.h. This
 *             definition can be included in the makefile during 
 *             compilation.                
 *
 * Author(s):  John Moore (Argon Engineering  vxWorks news group)
 *
 * $Revision: 2.2 $
 * $Date: 2004/06/24 11:23:48 $
 *
 * $Author: luchini $ Code from Till Straumann
 *
 * $Log: alm_ppcDec.c,v $
 * Revision 2.2  2004/06/24 11:23:48  luchini
 * call sysPciInLong/sysPciOutLong instad of sysPciRead32/sysPciWrite32
 *
 * Revision 2.1  2004/06/22 09:22:17  luchini
 * powerPc timer chip alm routines
 *
 *
 * This software is copyrighted by the BERLINER SPEICHERRING 
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY. 
 * The following terms apply to all files associated with the 
 * software. 
 *
 * BESSY hereby grants permission to use, copy, and modify this 
 * software and its documentation for non-commercial educational or 
 * research purposes, provided that existing copyright notices are 
 * retained in all copies. 
 *
 * The receiver of the software provides BESSY with all enhancements, 
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, 
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING 
 * OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY 
 * DERIVATIVES THEREOF, EVEN IF BESSY HAS BEEN ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR 
 * A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS 
 * PROVIDED ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO 
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR 
 * MODIFICATIONS. 
 *
 * Copyright (c) 1997 by Berliner Elektronenspeicherring-Gesellschaft
 *                            fuer Synchrotronstrahlung m.b.H.,
 *                                    Berlin, Germany
 *
 *********************************************************************-*/

#define INCLUDE_ALM_DEBUG  /* for ALM_DEBUG_MSG() macro w/logMsg()   */
#include <vxWorks.h>       /* for OK and ERROR                       */
#include <intLib.h>        /* for intLock(),intUnlock()              */
#include <logLib.h>        /* for logMsg() prototype                 */
#include <vxLib.h>         /* for vxTimeBaseGet() prototype          */
#include <alm.h>           /* for alm_status global struct           */
#include <alm_ppcDec.h>    /* for freq_reg,base_cnt_reg,etc          */

#include <alm_debug.h>     /* for ALM_DEBUG_MSG()                    */
#include <alm_debug.c>     /* for alm_debug_msg(),alm_set_debug_msg()*/

/*+**************************************************************************
 *              External Prototype Declarations
 **************************************************************************-*/
 
IMPORT void   sysPciOutLong (UINT32, UINT32);
IMPORT UINT32 sysPciInLong (UINT32);
IMPORT UINT32 sysTimeBaseLGet(void);

/*+**************************************************************************
 *              Global Variables
 **************************************************************************-*/             
alm_status_ts         alm_status = {0,0,0,0,0,
                                    TIMER_INT_LEVEL,
                                    TIMER3_INT_VEC,
                                    0,0,0};

/*+**************************************************************************
 *
 * Function:	alm_get_stamp
 *
 * Description: The timebase register is 64-bits wide.This function
 *              returns the lower 32-bits of the timerbase register
 *              which is sync'ed to the MPIC clock that runs at 4.16MHz.
 *              The timestamp value is has a resolution of ????, but 
 *              the value is returned with a microsecond resolution.
 * 
 *              Note: The rate of the timestamp timer is set explicitly by the 
 *                     hardware and typically cannot be altered.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in microseconds
 *
 **************************************************************************-*/

unsigned long alm_get_stamp(void)
{    
    UINT32  timebaseTicksPerUsec = TIMEBASE_FREQ/USECS_PER_SECOND;
    return( sysTimeBaseLGet()/timebaseTicksPerUsec );
}


/*+**************************************************************************
 *
 * Function:	alm_get_stampD
 *
 * Description: This function is used for measuring longer times 
 *              that require the full 64-bit resolution. The value is 
 *              returned in seconds.
 *
 *              From chapter 1 page 15 (eg page 55) of the Motorola 
 *              The time base is a 64-it register (accessed as two 32-bit registers)
 *              that is incremented every four bus clock cycles; externam control of the 
 *              time base is proviede through the time base enable (TBEN) signal.
 *              The decrementer is a 32-bit register that generates a decrementer 
 *              interrupt exception after a programmable delay. The contents of the
 *              decrementer register are decremented once every four bus clock cycles,
 *              and the decrementer exception is generated as the count passes through
 *              zero.
 *
 *              For the motorola mv2400 the timebase register is on the HAWK
 *              Note: check to see if vxTimeBaseGet handles rollover 
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Timestamp value in seconds
 *
 **************************************************************************-*/

double alm_get_stampD(void)
{
    UINT32 tbu;   /* Upper 32-bit word of the timebase register (sec)  */
    UINT32 tbl;   /* Lower 32-bit word of the timebase register (nsec) */
    double val;
        
    vxTimeBaseGet(&tbu,&tbl);    /* Get 64-bit value, 400 nsec res */
    val = (tbu * 4294967296.0 + tbl) * TIMEBASE_PERIOD; 
    return(val);
}


/*+**************************************************************************
 *
 * Function:    alm_setup
 *
 * Description: Set up the counter to go off in <delay> microseconds.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:   delay  -  Demanded timer delay in microseconds.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

void alm_setup(unsigned long delay)
{
    int      lock_key = 0;
    UINT32   wt_val = 0;
    UINT32   rbk_val=0;
    UINT32   level=0;
   

    ALM_DEBUG_MSG(5,"alm_setup: Entering function.\n",0,0);             
    lock_key = intLock();
    alm_status.running = TRUE;

    /* Direct interrupt at no one */
    sysPciOutLong(dest_reg, 0);
    EIEIO;                                  /* synchronize */
  
    /* 
     * Setup the timer frequency register to 4.16 MHz. This register is used
     * to reportr the frequency in Hz of the clock source for the
     * global timers. Following reset, this register contains zero.
     * The system initialization code must initialize this register
     * to one-eighth the MPIC clock frequency. For the PHB implementation
     * of the MPIC, a typical value would be 0x7de290 
     * (which is 66/8 MHz or 8.28 MHz). However, for the mv2400 the
     * value is 0x3ef148 (which is 33/8 MHz or 4.16Mhz). 
     */
    wt_val = TIMER_FREQ;
    sysPciOutLong(freq_reg, wt_val);      /* set timer freq register  */
    EIEIO;                                /* force write to complete  */ 

    /* Verify that the write was successful */    
    rbk_val = sysPciInLong(freq_reg); 
    if ( wt_val != rbk_val ) 
       ALM_DEBUG_MSG(0,
                     "alm_setup: Set timer freq reg failed, wt=0x%x rbk=0x%x\n",
                     wt_val,rbk_val);
    /* 
     * Enable interrupts by setting the interrupt priority level for timer #3. 
     * Note: interrupt priority 0 is the lowest 
     * and 15 is the higest. An interrupt priority level
     * of 0 will disable interrupts.
     */    
    level = alm_status.int_pri;
    wt_val = ((level<<16) | (TIMER3_INT_VEC));
    sysPciOutLong(vec_pri_reg,wt_val);
    EIEIO;

    /* Verify that the write was successful */
    rbk_val = sysPciInLong(vec_pri_reg);
    rbk_val &= (PRIORITY_MASK | VECTOR_MASK);
    if ( wt_val != rbk_val ) 
      ALM_DEBUG_MSG(0,
                    "alm_setup: Set timer freq reg failed, wt=0x%x rbk=0x%x.\n",
                     wt_val,rbk_val);

    /* Direct the interrupt at processor 0 */
    wt_val = DESTINATION_CPU0;
    sysPciOutLong(dest_reg, wt_val);
    EIEIO;     /* force write to complete */

    /* Verify that the write was successful */
    rbk_val = sysPciInLong(dest_reg);            
    rbk_val &= DESTINATION_CPU_MASK;
    if ( wt_val != rbk_val )   
        ALM_DEBUG_MSG(0,
                      "alm_setup: Set timer #3 destination reg failed, wt=0x%x rbk=0x%x.\n",
                       wt_val,rbk_val);
    /* 
     * Setup the timer to go off after the specified number 
     * of microseconds and enable counting.
     */
    wt_val = (delay * 1000)/TIMER_PERIOD; /* (delay*1000)/240) */
    sysPciOutLong(base_ct_reg,wt_val);
    EIEIO;   /* synchronize */

    /* Verify that the write was successful */
    rbk_val = sysPciInLong(base_ct_reg);         /* read the timer basecount register */
    rbk_val &= ~TIMER_INHIBIT;
    if ( wt_val != rbk_val )   
      ALM_DEBUG_MSG(0,
                    "alm_setup: Set timer #3 basecount reg failed, wt=0x%x rbk=0x%x.\n",
                    wt_val,rbk_val);
    intUnlock(lock_key);

    ALM_DEBUG_MSG(5,"alm_setup: Leaving function.\n",0,0);
    return;
}


/*+**************************************************************************
 *
 * Function:	alm_reset
 *
 * Description:	This function sets the timer to its max value 
 *              and disables counting.
 *
 * Arg(s) In:	enb    - flag to enable (1) or inhibit counting.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):   Error code:
 *                           OK or ERROR
 *
 **************************************************************************-*/
long alm_reset(int enb)
{
    long   status=OK;
    UINT32 wt_val=TIMER_DISABLE;
    UINT32 rbk_val=0;
    

    ALM_DEBUG_MSG(5,"alm_reset: Entering function.\n",0,0);    
    /* 
     * Set the timer basecount register to it's maximum value.
     * and enable or disable count depending on the input
     * argument "enb".
     */
    if ( !enb ) 
      wt_val |= TIMER_INHIBIT;  /* make sure to disable counting */
    sysPciOutLong(base_ct_reg,wt_val);     
    EIEIO;   /* synchronize */

    /* Now, verify that the write was successful */
    rbk_val = sysPciInLong(base_ct_reg); 
    if ( wt_val != rbk_val ) {
      status = ERROR;
      ALM_DEBUG_MSG(0,
                   "alm_reset: Set timer #3 basecount reg failed, wt=0x%x rbk=0x%x.\n",
                    wt_val,rbk_val);
    }
    ALM_DEBUG_MSG(5,"alm_reset: Leaving function.\n",0,0);
    return(status);
}


/*+**************************************************************************
 *
 * Function:    alm_disable
 *
 * Description: Disable counting for this timer.
 *              Mutex must be locked when using this function.
 *
 *              Note: Considering how the alm_disable() function
 *                    is written for the ppc decrementer timer,
 *                    it would make sense to disable the timer
 *                    interrupt by setting the interrupt level
 *                    to zero here.  However, to make this function
 *                    compatible with the timer functions written
 *                    for the mcc timer chip on the mv162 processor,
 *                    while keeping in mind how these functions have been
 *                    called in existing modules, this function 
 *                    inhibits counting for timer #3 only.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

void alm_disable(void)
{
    int     lock_key=0;
    UINT32  val=0;
    

    ALM_DEBUG_MSG(5, "alm_disable: Entering function.\n",0,0);

    lock_key = intLock();  
    if (alm_status.running) {
        /* Set Count Inhibit in the Base Count register.*/
        val = TIMER_DISABLE;
        sysPciOutLong(base_ct_reg, val);
        EIEIO;                                  /* synchronize */  
    }
    intUnlock(lock_key); 
 
    ALM_DEBUG_MSG(5, "alm_disable: Leaving function.\n",0,0);
    return;
}


/*+**************************************************************************
 *
 * Function:    alm_enable
 *
 * Description: Enable the timer interrupts, but does not start.
 *              the counter.
 *
 *              Note: Considering how the alm_enable() function
 *              is written for the ppc decrementer timer it would
 *              make sense to set delay in the base count register
 *              and start the counter. However to make this 
 *              function compatible with the timer functions written
 *              for the mcc timer chip on the mv162 processor,
 *              while keeping in mind how these functions have
 *              been called in existing modules, this function only 
 *              sets the interrupt level and vector. and directs the
 *              interrupt to cpu 0.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   Error code:
 *                           OK or ERROR
 *
 **************************************************************************-*/

long alm_enable(void)
{
    long   status = OK;
    UINT32 wt_val =0; 
    UINT32 rbk_val=0;
    UINT32 level=0;

   
    ALM_DEBUG_MSG(5, "alm_enable: Entering function.\n",0,0);

    /* Direct interrupt at no one */
    sysPciOutLong(dest_reg, 0);
    EIEIO;                                  /* synchronize */

    /* 
     * Set the interrupt priority level to 1 (disable) 
     * for timer #3. Please note that interrupt
     * priority 0 is the lowest and 15 is the higest.
     *
     * Note: the src/drv/timer/ppcDecTimer.c 
     * example timer code uses priority level 15.
     */
    level = alm_status.int_pri;
    wt_val = ((level<<16) | (TIMER3_INT_VEC));
    sysPciOutLong(vec_pri_reg,wt_val);
    EIEIO;  
 
    /* Now verify that the write succeeded. */
    rbk_val = sysPciInLong(vec_pri_reg);
    rbk_val &= (PRIORITY_MASK | VECTOR_MASK);
    if ( wt_val != rbk_val ) {
      status = ERROR;
      ALM_DEBUG_MSG(0,
                   "alm_enable: Failed to set timer #3 vector priority reg, wt=0x%x rbk=0x%x.\n",
                    wt_val,rbk_val);          
    } 
    
    /* Direct interrupt at CPU 0 */
    sysPciOutLong(dest_reg, DESTINATION_CPU0);
    EIEIO;                                  /* synchronize */
   

    ALM_DEBUG_MSG(5,"alm_enable: Leaving function.\n",0,0);
    return(status);
}


/*+**************************************************************************
 *
 * Function:    alm_freq
 *
 * Description: Returns the frequency in Hz of the clock used to
 * increment the time-base registers and to decrement the decrementer. 
 *      
 * Note: The rate of the timestamp timer is set explicitly by the 
 * hardware and typically cannot be altered.
 *
 * Arg(s) In:   None.
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   The alarm clock frequency in Hz.
 *
 **************************************************************************-*/

unsigned long alm_freq (void)
{
   return( TIMER_FREQ );  
}


/*+**************************************************************************
 *
 * Function:    alm_int_ack
 *
 * Description: This function is called to disable the counter
 *              once an interrupt exception has occured.
 *              
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   None.
 *
 **************************************************************************-*/

void alm_int_ack(void)
{
    /* Inhibit counting for this timer */
    sysPciOutLong(base_ct_reg,TIMER_DISABLE);
    EIEIO;
    return;   
}


/*+**************************************************************************
 * 
 * Function:    alm_intVecGet
 *
 * Description: This routine returns the interupt vector table offset
 *              currently used for timer #3. 
 *
 * Arg(s) In:   None.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector offset
 *
 **************************************************************************-*/ 

unsigned long alm_intVecGet(void)
{
    UINT32        rd_val=0;
    unsigned long vector=0; 

    rd_val = sysPciInLong(vec_pri_reg);   
    vector = rd_val & VECTOR_MASK;
    return(vector);
}


/*+**************************************************************************
 * 
 * Function:    alm_intLevelGet
 *
 * Description: This routine returns interrupt vector priority level.
 *              currently set.
 *
 * Arg(s) In:   None
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector priority level (1-7).
 *
 **************************************************************************-*/ 
int alm_intLevelGet(void)
{
    UINT32        rd_val=0;
    unsigned long level=0;

    /* read the timer vector/priority register */
    rd_val = sysPciInLong(vec_pri_reg);   
    level = (rd_val & PRIORITY_MASK) >> 16;
    return((int)level);
}


/*+**************************************************************************
 *
 * Function:    alm_int_delay_adjust
 *
 * Description: This function is called during alarm initialzation
 *              by the user to obtain the interrupt dealy adjustment.
 *
 * Arg(s) In:   None
 *
 * Arg(s) Out:  None.
 *
 * Return(s):   interrupt delay adjustment value
 *
 **************************************************************************-*/

unsigned long alm_int_delay_adjust(void)
{
    ALM_DEBUG_MSG(5,"alm_int_delay_adjust: Timer interrupt delay adjust is 0.\n",0,0);
    return(0);
}
