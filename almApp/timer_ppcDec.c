static char RcsId[]  = "@(#)$Id: timer_ppcDec.c,v 2.2 2004/06/24 11:23:57 luchini Exp $";

/*+*********************************************************************
 *
 * Time-stamp: <04 Jun 04 11:52:31 Kristi Luchini>
 *
 * File:       timer_ppcDec.c
 * Project:    
 *
 * Descr.:     
 *
 * Author(s):  John Moore (Argon Engineering  vxWorks news group)
 *
 * $Revision: 2.2 $
 * $Date: 2004/06/24 11:23:57 $
 *
 * $Author: luchini $
 *
 * $Log: timer_ppcDec.c,v $
 * Revision 2.2  2004/06/24 11:23:57  luchini
 * call sysPciInLong/sysPciOutLong instad of sysPciRead32/sysPciWrite32
 *
 * Revision 2.1  2004/06/22 09:21:49  luchini
 * powerPc timer chip test routines
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
 
#include "vxWorks.h"       /* for OK and ERROR              */
#include "stdio.h"         /* for printf() prototype        */
#include "semLib.h"        /* for semBCreate() prototype    */
#include "string.h"        /* for memset() prototype        */
#include "sysLib.h"        /* for sysClkRateGet() prototype */
#include "intLib.h"        /* for intConnect() prototype    */
#include "iv.h"            /* INUM_TO_IVEC() macro          */
#include "logLib.h"        /* for logMsg() prototype        */
#include "taskLib.h"       /* for taskSpawn() prototype     */
#include "vxLib.h"         /* for vxTimeBaseGet() prototype */
#include "timer_ppcDec.h"  /* for timer_isr(),etc           */
#include "alm_ppcDec.h"    /* for base_ct_reg,etc.          */

/*+**************************************************************************
 *              External Prototype Declarations
 **************************************************************************-*/

IMPORT void   sysPciOutLong(UINT32, UINT32);
IMPORT UINT32 sysPciInLong(UINT32);
IMPORT UINT32 sysTimeBaseLGet(void);

/*+**************************************************************************
 *             Local Variables
 **************************************************************************-*/ 

#define MAX_CNT 10
static UINT32  cur_delay=0;
static UINT32  start_time=0;
static UINT32  finish_time=0;
static INT32   diff_time_a[MAX_CNT];
static UINT32  cnt=0;
static short   timer_running=FALSE;
static SEM_ID  semId=0;



/*====================================================

  Abs:  Initialize powerPC decrementer timer #3
        and to start the timer.

  Name: timer_init

  Args: delay                       delay in microseconds
          Use:  integer             
          Type: unsigned long
          Acc:  read-only
          Mech: By value

 
  Rem:  The purpose of this function is to configure
        the powerPC decrementer interruptable timer #3.
        This routine should be called more than once,
	however, the interrupt handler is set on the
	first call to this function.

   
  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_init(unsigned long delay)
{
    long    status=OK;
    UINT32  val=0; 

    if (timer_running) {
      logMsg("Timer already running\n",0,0,0,0,0,0);
      return(ERROR);
    }
    timer_running = TRUE;
    start_time=0;
    finish_time=0; 
    cur_delay=delay;
    cnt = 0;
    memset((void *)diff_time_a,0,sizeof(diff_time_a));

    /* First, disable the timer. */
    sysPciOutLong(base_ct_reg,TIMER_DISABLE);
    EIEIO;
    
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
    sysPciOutLong(freq_reg, TIMER_FREQ);
    EIEIO;

    /* set the priority level and vector for timer #3 */
    val = PRIORITY_LVL15 | (TIMER3_INT_VEC);
    sysPciOutLong(vec_pri_reg,val);
    EIEIO;

    /* set the destination of the interrupt to be CPU 0 */
    sysPciOutLong(dest_reg, DESTINATION_CPU0);
    EIEIO;

    /* connect the scenario tick routine to timer3 */
    if ( intVecGet((FUNCPTR *)INUM_TO_IVEC(TIMER3_INT_VEC))==NULL ) {
       status = intConnect(INUM_TO_IVEC(TIMER3_INT_VEC),(VOIDFUNCPTR)timer_isr,0);
       if (status!=OK)
           printf("Failed to connect timer interrupt vector 0x%x to ISR.\n",
                  TIMER3_INT_VEC);
       else {   
         semId = semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);
         taskSpawn("t_timer",40,0,3000,(FUNCPTR)timer_report,0,0,0,0,0,0,0,0,0,0);
       } 
    }

    if (status==OK) {
      /* 
       * Setup the specified number of milliseconds delay in
       * the base count register and enable counting.
       */
      logMsg("\nStarting timer with a delay of %d microseconds\n",delay,0,0,0,0,0);
      val =  (delay * 1000)/TIMER_PERIOD;    /* (delay*1000)/240) */
      start_time = sysTimeBaseLGet();
      sysPciOutLong( base_ct_reg, val);
      EIEIO;
      logMsg("\nSet base count register to %d counts\n",val,0,0,0,0,0);
    }
    return(status);
}

/*====================================================

  Abs:  Timer interrup service routine

  Name: timer_isr

  Args: None
 
  Rem:  This function is envoked when an interrupt is 
        generated by the timer #3 chip. This occurs
	when the decrement counter has reached zero.
	
	This function reads the current time and
	calculates the time since the last interrupt
	was recieved. After MAX_CNT interrupts the
	timer #3 interrupt is disabled and the
	routine to report the statisticts taken (times)
	is envoked before exiting.
 
  Side: None

  Ret:  None
               
=======================================================*/
void timer_isr(void)
{           
   /* 
    * The PowerPC Timebase Lower (TBL) register is 32 bits wide and serves
    * as the timestamp clock. The timebase register has a precision of 60ns.
    */      
    finish_time = sysTimeBaseLGet();
    diff_time_a[cnt++] = finish_time - start_time;
    if ( cnt < MAX_CNT )
      start_time = finish_time;
    else { 
      sysPciOutLong(base_ct_reg,TIMER_DISABLE);
      EIEIO;
      timer_running=FALSE;
      logMsg("Stop timer\n",0,0,0,0,0,0); 
      semGive(semId);    
    }
    return;   
}

/*====================================================

  Abs:  Report the statisticts taken by the timer ISR

  Name: timer_report

  Args: None

  Rem:  The purpose of this function is print to the
        standard output device, the statistics taken
	during by the interrupt handler, timer_isr().

  Side: None

  Ret:  None
               
=======================================================*/
void timer_report(void)
{
    int     done = FALSE;
    int     i = 0;
    UINT32  usec=0;
    UINT32  timebaseTicksPerUsec = TIMEBASE_FREQ/USECS_PER_SECOND;

    while (!done) {   
      semTake(semId,WAIT_FOREVER);  

      /* The timebase register has a resolution of 60ns */
      for ( i=0; i< MAX_CNT; i++ ) {
         usec = diff_time_a[i]/timebaseTicksPerUsec;
         logMsg("Time Elapsed %d: is %d usec\n",i,usec,0,0,0,0);
      }  
    }
    return;
}


/*====================================================

  Abs:  Set the timer base counter reg w/# of 
        microseconds delay.
  
  Name: timer_delay_usec
  
  Args: delay                       delay in microseconds
          Use:  integer             
          Type: unsigned long
          Acc:  read-only
          Mech: By value

 
  Rem:  The purpose of this function is to set the timer
        base count register to the number of counts
	that reflect the specified delay in microseconds.
   
  Side: None

  Ret:  unsigned long  
             Time the counter was set
               
=======================================================*/ 
UINT32 timer_delay_usec(unsigned long delay)
{
   UINT32  val=0; 

   val = delay * MICRO_SECOND;
   sysPciOutLong(base_ct_reg, val);
   EIEIO;
   
   start_time = sysTimeBaseLGet();
   cur_delay  = delay;          
   printf("Resetting timer delay to %ld microseconds\n",delay);       
   return(start_time);
}
           

/*====================================================

  Abs:  Set the timer base counter reg w/# of 
        milliseconds delay.

  Name: timer_delay_msec

  Args: delay                       delay in milliseconds
          Use:  integer             
          Type: unsigned long
          Acc:  read-only
          Mech: By value


  Rem:  The purpose of this function is to set the timer
        base count register to the number of counts
	that reflect the specified delay in milliseconds.
   
  Side: None

  Ret:  unsigned long  
            time the counter register was set
               
=======================================================*/
UINT32 timer_delay_msec(unsigned long delay)
{
   UINT32  val=0; 

   val = delay * MILLI_SECOND;
   sysPciOutLong(base_ct_reg, val);
   EIEIO;
   
   start_time = sysTimeBaseLGet();
   cur_delay  = delay;          
   printf("Resetting timer delay to %ld milliseconds\n",delay);       
   return(start_time);
}

/*====================================================

  Abs:  Enable the timer counter.

  Name: timer_start

  Args: None

  Rem:  The purpose of this function is to enable
        the timer counter.
   
  Side: The isr should already be setup in the vector table.

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_start(void)
{
    long   status=OK;
    UINT32 wt_val=0;
    UINT32 rd_val=0;
    
    rd_val = sysPciInLong(base_ct_reg);

    /* start counter by clearing inhibit bit */
    wt_val &= ~TIMER_INHIBIT;  
    sysPciOutLong(base_ct_reg,wt_val);
    EIEIO;   /* synchronize */
    return(status);   
}

/*====================================================

  Abs:  Disable or inhibit the timer counter.


  Name: timer_stop

  Args: None
 
  Rem:  The purpose of this function is to inhibit
        the timer counter.
  
  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_stop(void)
{
    long   status=OK;
    UINT32 wt_val=0;
    UINT32 rd_val=0;
   
    rd_val = sysPciInLong(base_ct_reg);

   /* stop counting by setting inhibit bit */
    wt_val |= TIMER_INHIBIT;  
    sysPciOutLong(base_ct_reg,wt_val);
    EIEIO;   /* synchronize */
    return(status);
}
   
/*====================================================

  Abs:  Enable the timer interrupts

  Name: timer_int_enable

  Args: None

  Rem:  The purpose of this function is to enable
        the timer interrupt by clearing the interrupt
	enable bit in the timer vector/priority register.
   
  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_int_enable(void)
{
    long   status=OK;
    UINT32 wt_val=0;
    UINT32 rd_val=0;

    rd_val = sysPciInLong(vec_pri_reg);
    wt_val = rd_val & ~INT_MASK_BIT;
    sysPciOutLong(vec_pri_reg,wt_val);
    return(status);   
}

/*====================================================

  Abs:  Disable timer interrupts

  Name: timer_int_disable

  Args: None
 
  Rem:  The purpose of this function is to disable
        the timer interrupts but setting the interrupt
	enable bit in the vector/priority timer register.

  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_int_disable(void)
{
    long   status=OK;
    UINT32 wt_val=0;
    UINT32 rd_val=0;

    rd_val = sysPciInLong(vec_pri_reg);
    wt_val = rd_val | INT_MASK_BIT;
    sysPciOutLong(vec_pri_reg,wt_val);
    return(status);  
}

/*====================================================

  Abs:  Read the timer current count register

  Name: timer_rd_ccnt

  Args: None
 
  Rem:  The purpose of this function is read the
        timer current count register.
   
  Side: None

  Ret:  unsigned long  
            value of current count register read.
               
=======================================================*/
unsigned long timer_rd_ccnt(void)
{
    UINT32 rd_val=0;
    UINT32 toggle=0;
    UINT32 cnt=0;

    rd_val = sysPciInLong(cur_cnt_reg);
    toggle = rd_val >> 31;
    cnt = rd_val & ~TIMER_INHIBIT;
    logMsg("Current count is 0x%lx (%ld)  Inhibit state=%d)\n",
           cnt,cnt,toggle,0,0,0);
    return(rd_val);
}

/*====================================================

  Abs:  Read timer basecount register

  Name: timer_rd_bcnt

  Args: None

 
  Rem:  The purpose of this function is to read
        the timer basecount register.
	configure
   
  Side: None

  Ret:  unsigned long  
            value of the basecount register read.
               
=======================================================*/
unsigned long timer_rd_bcnt(void)
{
    UINT32 rd_val=0;
    UINT32 cnt_inhibit=0;
    UINT32 cnt=0;
    
    rd_val = sysPciInLong(base_ct_reg);
    cnt_inhibit = rd_val >> 31;
    cnt = rd_val & ~TIMER_INHIBIT;
    logMsg("Base count is 0x%lx (%ld)  (%s)\n",
           cnt,
           cnt,
           (int)((cnt_inhibit) ? "Counting Inhibited" : "Counting"),
           0,0,0);
    return(rd_val);
}

/*====================================================

  Abs:  Read the timer vector/priority register

  Name: timer_rd_vec

  Args: None

  Rem:  The purpose of this function is to read
        the timer vector/prioroty register.
   
  Side: None

  Ret:  unsigned long  
            value of the vector/priority register read.
               
=======================================================*/
unsigned long timer_rd_vec(void)
{
    UINT32 rd_val=0;
    UINT32 vector=0;
    UINT32 level=0;
    
    rd_val = sysPciInLong(vec_pri_reg);
    vector = rd_val & VECTOR_MASK;
    level  = (rd_val & PRIORITY_MASK) >> 16;
    logMsg("Interrupt vector is 0x%lx (%ld) level is %ld\n",
           vector,vector,level,0,0,0);
    return(rd_val);
}

/*====================================================

  Abs:  Read the timer destination register.

  Name: timer_rd_dest

  Args: None

  Rem:  The purpose of this function is to read the
        timer destination register.

  Side: None

  Ret:  unsigned long  
            value of the destination register read.
               
=======================================================*/
unsigned long timer_rd_dest(void)
{
    UINT32 rd_val=0;

    rd_val = sysPciInLong(dest_reg);
    return(rd_val);
}

/*====================================================

  Abs:  Read the timer frequency register

  Name: timer_rd_freq

  Args: None
 
  Rem:  The purpose of this function is to read the
        timer frequency register. 
   
  Side: Note this register is only informational.

  Ret:  unsigned long  
            value of the timer frequency register read.
               
=======================================================*/
unsigned long timer_rd_freq(void)
{
    UINT32 rd_val=0;

    rd_val = sysPciInLong(freq_reg);    
    return(rd_val);
}


/*====================================================

  Abs:  Set timer basecount register for the specified 
        # of microseconds (and start counting).

  Args: delay                       delay in microseconds
          Use:  integer             
          Type: unsigned long
          Acc:  read-only
          Mech: By value

 
  Rem:  The purpose of this function is to to set
        the timer basecounter register for the
	specified number of microseconds, and to
	enable counting.
   
  Side: see also timer_delay_usec()

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_set_delay(unsigned long delay)
{
   long    status=OK;
   UINT32  wt_val=0; 

   wt_val = delay * MICRO_SECOND;
   sysPciOutLong(base_ct_reg,wt_val);
   EIEIO;
   logMsg("Resetting timer delay to %d microseconds\n",delay,0,0,0,0,0);       
   return(status);
}

/*====================================================

  Abs:  Set the timer frequency register.

  Name: timer_set_freq

  Args: None
 
  Rem:  The purpose of this function is to to set
        the timer frequency register to the appropriate
	value. You will have to review the documentation
	for your powerPC to find out the correct value.
   
  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_set_freq(void)
{
    long   status = OK;
    UINT32 wt_val = TIMER_FREQ;
    UINT32 rbk_val=0;

   /* 
    * Set the timer frequency register to 4.12 MHz
    * Note: PCI bus clock, ie: 33Mhz/8=4.12MHz
    */
    sysPciOutLong(freq_reg,wt_val);       /* set timer freq register  */
    EIEIO;                                /* force write to complete  */ 
    logMsg("Set timer frequency to %d.\n",wt_val,0,0,0,0,0);

    /* Verify that the write was successful */    
    rbk_val = sysPciInLong(freq_reg);      /* read timer freq register */
    if ( wt_val != rbk_val ) {
        status = ERROR;
        logMsg("Set timer freq reg failed, wt=x%0x rbk=0x%x.\n",
               wt_val,rbk_val,0,0,0,0);
    }   
    return(status);
}


/*====================================================

  Abs:  Set the timer base count register enb or disable
        counting  according to flag.

  Name: timer_set_bcnt

  Args: val                      Value to set in the
          Use:  integer          timer base count register
          Type: unsigned long    (xount value fields only).
          Acc:  read-only
          Mech: By value


        enb                      Enable flag indicates 
          Use:  integer          if the counter should
          Type: int              be enabled if set. If
          Acc:  read-only        0 the counter will be
          Mech: By value         disabled.
	   
  Rem:  The purpose of this function is to write
        the specified value to the timer base
	count register.

  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
long timer_set_bcnt(unsigned long val,int enb)
{
    long   status=OK;    
    UINT32 rd_val=0;
    
    /* Set timer counter and let it fly */
    sysPciOutLong(base_ct_reg,val);
    rd_val = sysPciInLong(base_ct_reg);
    if ( val != rd_val ) {
        logMsg("Base count write failed wt=0x%x rbk=0x%x\n",
               val,rd_val,0,0,0,0);
        status=ERROR;
    } 
    return(status);
}


/*====================================================

  Abs:  Read the full 64-bit timebase register 

  Name: readTimebase

  Args: None
 
  Rem:  The purpose of this function is for measuring
        longer times than seconds by using  the full 
	64-bit resolution. The time that has passed is
	returned in seconds.
 
        From chapter 1 page 15 (eg page 55) of the Motorola 
        The time base is a 64-it register (accessed as two 32-bit registers)
        that is incremented every four bus clock cycles; externam control of the 
        time base is proviede through the time base enable (TBEN) signal.
        The decrementer is a 32-bit register that generates a decrementer 
        interrupt exception after a programmable delay. The contents of the
        decrementer register are decremented once every four bus clock cycles,
        and the decrementer exception is generated as the count passes through
        zero.

       For the motorola mv2400 the timebase register is on the HAWK
  
  Side: None

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
double readTimebase(void)
{
        UINT32 tbu;   /* Upper 32-bit word of the timebase register (sec)  */
        UINT32 tbl;   /* Lower 32-bit word of the timebase register (nsec) */
        double val;
        
        vxTimeBaseGet(&tbu,&tbl);          /* Get 64-bit value */
        val = (tbu * 4294967296.0 + tbl) * TIMEBASE_PERIOD;
        return(val);
}

/*====================================================

  Abs:  Get time passes base on tickes 

  Name: benchmark_double

  Args: nticks                  Number of clocks to delay
          Use:  integer             
          Type: int
          Acc:  read-only
          Mech: By value

 
  Rem:  The purpose of this function is read the time
        stamp from the full 64-bits of the time base
	register, wait the specified number of tickes
	and then read the timestamp again. The difference
	between the start and end time is calcuated and
	the number of microseconds that have passed is
	printed to the standard output. Basically, the
	number of microseconds that has passed should
	be fairly close to what the number of ticks
	workes out to in microseconds and seconds. 
	
  Side:	This function provides only a rough estimate...and 
	the users should note that nothing else should be 
	running at the time that this test takes place
        as taskDelay may not work reliably.
   
  Ref: see benchmark

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
void benchmark_double(int nticks)
{
     double ts,te,td;
    
     ts = readTimebase();
     taskDelay(nticks);
     te = readTimebase();
     td = te - ts;

     printf("Elapsed time = %f sec\n", td);
     printf("             = %f usec\n", td * 1.0e6);
}

/*====================================================

  Abs:  Get time passed in microseconds based on delay

  Name: benchmark

  Args: nticks                  Number of clocks to delay
          Use:  integer             
          Type: int
          Acc:  read-only
          Mech: By value

 
  Rem:  The purpose of this function is read the time
        stamp from the lower 32-bits of the time base
	register, wait the specified number of tickes
	and then read the timestamp again. The difference
	between the start and end time is calcuated and
	the number of microseconds that have passed is
	printed to the standard output. Basically, the
	number of microseconds that has passed should
	be fairly close to what the number of ticks
	workes out to in microseconds and seconds.
	
  Side:	This function provides only arough estimate...and 
	the users should note that nothing else should be 
	running at the time that this test takes place
        as taskDelay may not work reliably.
   
  Ref: see benchmark_double

  Ret:  long  
            OK    - Successful operation
            ERROR - Operation failed
               
=======================================================*/
void benchmark(int nticks)
{
     UINT32  ts,te,td;
     UINT32  timebaseTicksPerUsec = TIMEBASE_FREQ/USECS_PER_SECOND; 

     ts = sysTimeBaseLGet();     
     taskDelay(nticks);;     
     te = sysTimeBaseLGet();    
     td = te - ts;

     printf("Elapsed time = %d usec\n", td/timebaseTicksPerUsec);
     return;
}
