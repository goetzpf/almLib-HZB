/*+*********************************************************************
 *
 * Time-stamp: <16 Jun 04 13:46:55 Kristi Luchini>
 *
 * File:       alm_ppcDec.h
 * Project:    
 *
 * Descr.:     
 *
 * Author(s):  John Moore (Argon Engineering  vxWorks news group)
 *
 * $Revision: 2.1 $
 * $Date: 2004/06/22 09:20:05 $
 *
 * $Author: luchini $ Code from Kristi Luchin
 *
 * $Log: alm_ppcDec.h,v $
 * Revision 2.1  2004/06/22 09:20:05  luchini
 * header file for powerPc alm routines
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

#ifndef __ALM_PPCDEC_H
#define __ALM_PPCDEC_H

#ifdef __cplusplus
extern "C" {
#endif

#define INCLUDE_PCI		/* always include pci           */
#include "config.h"             /* for configuration parameters */
#define INCLUDE_PCI_AUTOCONF
#if (CPU == PPC604)
#ifdef INCLUDE_MPIC
#include <hawkMpic.h>
#endif
#else  /* (CPU == PPC603) */
#include <kahluaEpic.h>
#endif
 
/*+**************************************************************************
 *              Prototype definition
 **************************************************************************-*/

UINT32        sysGetBusSpd(void);
UINT          sysGetPciSpd(void);             /* pci bus speed (in hertz) */

/*+**************************************************************************
 *              Macro definition
 **************************************************************************-*/

/*
 * Common I/O synchronizing instructions .
 * Force all writes to complete
 */
#ifndef EIEIO  
 #define EIEIO __asm__ volatile ("eieio")   
#endif 

/*+**************************************************************************
 *              Parameter Definitions
 **************************************************************************-*/
#define MSECS_PER_SECOND          1000
#define USECS_PER_SECOND          1000000
#define NSECS_PER_SECOND          1000000000

/*
 * From chapter 1 page 15 (eg page 55) of the Motorola 
 * The time base is a 64-it register (accessed as two 32-bit registers)
 * that is incremented every four bus clock cycles; externam control of the 
 * time base is proviede through the time base enable (TBEN) signal.
 * The decrementer is a 32-bit register that generates a decrementer 
 * interrupt exception after a programmable delay. The contents of the
 * decrementer register are decremented once every four bus clock cycles,
 * and the decrementer exception is generated as the count passes through
 * zero.
 *
 * For the motorola mv2400 the timebase register is on the HAWK
 */

#if (CPU == PPC604)
#define TIMEBASE_DECREMENTER_PRESCALE   4
#else  /* (CPU == PPC603) */
#define TIMEBASE_DECREMENTER_PRESCALE   10
#endif

/* 
 * Please note that the function sysTimestampFreq() can be used
 * instead of TIMEBASE_FREQ. The prototype for this function is
 * in drv/timer/timerDev.h
 */
#define TIMEBASE_FREQ             (DEC_CLOCK_FREQ/TIMEBASE_DECREMENTER_PRESCALE) 
#define TIMEBASE_PERIOD           (1.0/TIMEBASE_FREQ)             /* period in seconds */
#define TIMEBASE_PERIOD_NSEC      (NSECS_PER_SECOND/TIMEBASE_FREQ) /* 400nsec per tick */

/* 
 * The pci timers run at a subharmonic of the PCI bus clock.
 * To calculate number of ticks equal to 1 microsecond the
 * Time Base register and the Decrementer count at the same rate:
 * once per 8 System Bus cycles.
 *
 *  ticks/Usec = (pci bus frequency / decrementer pre-scaler) / 1000000
 *
 *
 * e.g., 3333333 cycles      1 tick      1 second             4.16 tick
 *       ---------------  *  ------   *  --------          =  ----------
 *       second              8 cycles    1000000 microsec     microsec  
 *
 *       33333333 / 8 = 4.16Mhz
 *
 */
#define TIMER_DECREMENTER_PRESCALE  8
#define TIMER_FREQ                (sysGetPciSpd()/TIMER_DECREMENTER_PRESCALE) 
#define TIMER_PERIOD              (NSECS_PER_SECOND/TIMER_FREQ)    /* 240 nsecs per tick */

#define MILLI_SECOND              (TIMER_FREQ/MSECS_PER_SECOND)    /* freq in milliseconds    */  
#define MICRO_SECOND              (TIMER_FREQ/USECS_PER_SECOND)    /* freq in micro seconds   */
#define NANO_SECONDS              (TIMER_FREQ/NSECS_PER_SECOND)

#define TIMER_DISABLE              0xFFFFFFFF          /* disable & reset the timer ctr val   */
#define TIMER_INT_LEVEL            2

/* 
 * timer registers 0-3
 * - current count register
 * - base count register
 * - vector/priority register
 * - destination register
 *
 * In addition to the per-timer registers listed above, there is a single Timer
 * Frequency Register which is used to hold the effective clock frequency of the
 * MPIC timer clock frequency. Start-up code must initialize this 32-bit register
 * to the PCI clock frequency divided by 8.
 *
 * NOTE: Raven MPIC was clocked by the 60x bus clock. Hawk MPIC is clocked
 *       by the PCI bus clock.
 */
#define DESTINATION_CPU1     0x2
#define DESTINATION_CPU_MASK (DESTINATION_CPU0 | DESTINATION_CPU1)

#if (CPU == PPC604)
#define TIMER3_INT_VEC     (TIMER0_INT_VEC+3)
static UINT32 freq_reg    = MPIC_BASE_ADRS + MPIC_TIMER_FREQ_REG;      /* frequency register                 */
static UINT32 cur_cnt_reg = MPIC_BASE_ADRS + MPIC_TIMER3_CUR_CNT_REG;  /* current count register             */
static UINT32 base_ct_reg = MPIC_BASE_ADRS + MPIC_TIMER3_BASE_CT_REG;  /* base count register                */
static UINT32 vec_pri_reg = MPIC_BASE_ADRS + MPIC_TIMER3_VEC_PRI_REG;  /* interrupt vector priority register */
static UINT32 dest_reg    = MPIC_BASE_ADRS + MPIC_TIMER3_DEST_REG;     /* destination register               */

#else /* (CPU == PPC603) */
static UINT32 freq_reg    = (UINT32)EPIC_TIMER_FREQ_REG;          /* frequency register                 */
static UINT32 cur_cnt_reg = (UINT32)EPIC_TIMER3_CUR_CNT_REG;      /* current count register             */
static UINT32 base_ct_reg = (UINT32)EPIC_TIMER3_BASE_CT_REG;      /* base count register                */
static UINT32 vec_pri_reg = (UINT32)EPIC_TIMER3_VEC_PRI_REG;      /* interrupt vector priority register */
static UINT32 dest_reg    = (UINT32)EPIC_TIMER3_DEST_REG;         /* destination register               */
#endif

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALM_PPCDEC_H */
