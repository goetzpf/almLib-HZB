/* $RCSfile: almLib.c,v $ */

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	Timer - Timer and Alarm Clock Support
 *
 * File:	alm.c
 *
 * Description:	Library package to implement an alarm clock using the
 *              timer 4 on the mvme162'c MCC chip.
 *		(See also timestamp timer code in vw/src/drv/timer/xxxTS.c)
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 1.1 $
 * $Date: 1996/05/20 11:54:57 $
 *
 * $Author: lange $
 *
 * $Log: almLib.c,v $
 * Revision 1.1  1996/05/20 11:54:57  lange
 * Changed name (to avoid EPICS name conflicts).
 *
 * Revision 1.1  1996/04/23 17:31:08  lange
 * *** empty log message ***
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 **************************************************************************-*/

static char
rcsid[] = "@(#)mCAN-timer: $Id: almLib.c,v 1.1 1996/05/20 11:54:57 lange Exp $";


#include <vxWorks.h>
#include <stdlib.h>
#include <intLib.h>
#include <iv.h>

#define MCC_BASE_ADRS   (0xfff42000)             /* MCchip reg base address */
#define MCC_INT_VEC_BASE       0x40     /* MCC interrupt vector base number */

#include <drv/multi/mcchip.h>

#include <ts48.h>

#include "alm.h"


#define ALARMCLOCK_LEVEL 1	/* Alarm clock's interrupt level */

#define INT_INHIBIT 500L	/* Minimum interrupt delay */
#define LIST_CHUNK_SIZE 20	/* Number of elements in alarm list chunk */


/*+**************************************************************************
 *		DEBUG mode
 **************************************************************************-*/

#ifdef ALM_DEBUG

#include <stdio.h>
#include <semaphore.h>

#define ALM_DBG(verb, string) {				\
   if (verb <= dbg_level) {				\
      sem_wait(&alm_dbg_lock);				\
      printf(__FILE__ ":%d: %s\n", __LINE__, string);	\
      sem_post(&alm_dbg_lock);				\
   }							\
}

#define ALM_PRF(verb, x) {			\
   if (verb <= dbg_level) {			\
      sem_wait(&alm_dbg_lock);			\
      printf(__FILE__ ":%d: ", __LINE__);	\
      printf x;					\
      sem_post(&alm_dbg_lock);			\
   }						\
}

#define ASSERT_STRUC(str, type) {				\
   if ((str)->magic1 != type##_MAGIC1)				\
      if (1 <= dbg_level) {					\
	 sem_wait(&alm_dbg_lock);				\
	 printf(__FILE__ ":%d: PANIC: " #type			\
		" structure head corrupt.\n", __LINE__);	\
	 sem_post(&alm_dbg_lock);				\
      };							\
   if ((str)->magic2 != type##_MAGIC2)				\
      if (1 <= dbg_level) {					\
	 sem_wait(&alm_dbg_lock);				\
	 printf(__FILE__ ":%d: PANIC: " #type			\
		" structure tail corrupt.\n", __LINE__);	\
	 sem_post(&alm_dbg_lock);				\
      };							\
}

#define SET_MAGIC(str, type) {			\
   (str)->magic1 = type##_MAGIC1;		\
   (str)->magic2 = type##_MAGIC2;		\
}

   
#define Alarm_Entry_MAGIC1 0x12345601
#define Alarm_Entry_MAGIC2 0x12345602
#define MAGIC1 unsigned long magic1
#define MAGIC2 unsigned long magic2

#else  /* NOT ALM_DEBUG */

#define ALM_DBG(verb, string)
#define ALM_PRF(verb, x)
#define ASSERT_STRUC(str, type)
#define SET_MAGIC(str, type)
#define MAGIC1
#define MAGIC2

#endif /* NOT ALM_DEBUG */


/*+**************************************************************************
 *		Mutual Exclusion
 **************************************************************************-*/

#ifndef DECLARE_LOCK
				/* Mutual exclusion mechanism */
#include <semaphore.h>
#include <stdio.h>
#define DECLARE_LOCK(name) static sem_t name
#define INIT_LOCK(name) sem_init(&name, 0, 1)
#if 1
#define LOCK(name) {						\
   sem_wait(&name);						\
   if (4 <= dbg_level) {					\
      sem_wait(&alm_dbg_lock);					\
      printf(__FILE__ ":%d: " #name " locked.\n", __LINE__);	\
      sem_post(&alm_dbg_lock);					\
   }								\
}
#define UNLOCK(name) {						\
   sem_post(&name);						\
   if (4 <= dbg_level) {					\
      sem_wait(&alm_dbg_lock);					\
      printf(__FILE__ ":%d: " #name " unlocked.\n", __LINE__);	\
      sem_post(&alm_dbg_lock);					\
   }								\
}

#else
#define LOCK(name) sem_wait(&name)
#define UNLOCK(name) sem_post(&name)
#endif

#endif /* ndef DECLARE_LOCK */


/*+**************************************************************************
 *		Types
 **************************************************************************-*/

typedef struct a_node
{
   MAGIC1;
   ts48_Stamp     time_due;	/* Alarm time */
   sem_t*         sem_p;	/* Semaphore to give */
   struct a_node* next_p;
   MAGIC2;
} Alarm_Entry;


/*+**************************************************************************
 *		Globals
 **************************************************************************-*/

static struct 
{
   unsigned int running : 1;	/* Counter running flag */
   unsigned int in_use  : 1;	/* Lists in use flag */
   unsigned int woke_up : 1;	/* Int handler was called flag */
} status = {FALSE, FALSE, FALSE};

static Alarm_Entry* alarm_list;	/* List of pending alarms */
static Alarm_Entry* free_list;	/* List of unused alarm entries */

DECLARE_LOCK(alm_lock);		/* Mutex semaphore */

#ifdef ALM_DEBUG
DECLARE_LOCK(alm_dbg_lock);	/* Declare debug semaphore */
static char dbg_level = -1;	/* Debug level (default=none) */
#endif


/*+**************************************************************************
 *		Functions
 **************************************************************************-*/

/* Discard an alarm (alarm list -> free list) */
static void alm_discard (Alarm_Entry* id);

/* Setup the timer to interrupt in <delay> microseconds */
static void alm_setup (unsigned long delay);

/* Stop (disable) the timer */
static void alm_stop (void);

/* The alarm counter's interrupt handler */
static void alm_int_handler (void);



/*+**************************************************************************
 *
 * Function:	alm_discard
 *
 * Description:	Put an alarm from the alarm list into the free list.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	id  -  Id of alarm to discard.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_discard (Alarm_Entry* id)
{
   register Alarm_Entry* last_p = NULL;
   register Alarm_Entry* act_p  = alarm_list;

   ALM_DBG(5, "Entering alm_discard.");

   if (act_p == NULL) {
      ALM_DBG(3, "alm_discard: alarm list empty.");
      return;	/* Return on empty alarm list */
   }

   do {
      if (act_p == id) break;
      
      last_p = act_p;		/* Go to next element */
      act_p  = act_p->next_p;
      
   } while (act_p != NULL);

   if (act_p == NULL) {
      ALM_DBG(3, "alm_discard: specified alarm not found.");
      return;	/* Alarm not found */
   }
   
   ASSERT_STRUC(act_p, Alarm_Entry);

   if (last_p != NULL)		/* Take alarm out of alarm list */
      last_p->next_p = act_p->next_p;
   else
      alarm_list = act_p->next_p;

   act_p->next_p = free_list;	/* Put unused entry into free list */
   free_list = act_p;

   ALM_PRF(3, ("alm_discard: took entry %p from alarm list to free list.",
	       act_p));
   ALM_DBG(5, "Leaving alm_discard.");
}


/*+**************************************************************************
 *
 * Function:	alm_setup
 *
 * Description:	Set up the counter to go off in <delay> microseconds.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	delay  -  Demanded timer delay in microseconds.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_setup (unsigned long delay)
{
   ALM_DBG(5, "Entering alm_setup.");

				/* Connect interrupt handler */
   (void) intConnect (INUM_TO_IVEC (MCC_INT_VEC_BASE + MCC_INT_TT4),
		      alm_int_handler, NULL);

   status.running = TRUE;	/* Set running flag */

				/* Reset & enable the alarm timer interrupt */
   *MCC_T4_IRQ_CR  = T4_IRQ_CR_IEN | T4_IRQ_CR_ICLR | ALARMCLOCK_LEVEL;

				/* Set the timer period */
   *MCC_TIMER4_CMP = (delay < INT_INHIBIT ? INT_INHIBIT : delay);
   
   *MCC_TIMER4_CNT = 0;		       /* Reset the counter */
   *MCC_TIMER4_CR  = TIMER4_CR_CEN;    /* Enable the timer */

   ALM_PRF(4, ("alm_setup: counter set to %d.\n", delay));
   ALM_DBG(5, "Leaving alm_setup.");
}


/*+**************************************************************************
 *
 * Function:	alm_stop
 *
 * Description:	Stop (disable) the counter.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_stop (void)
{
   ALM_DBG(5, "Entering alm_stop.");

   if (status.running) {
      *MCC_T4_IRQ_CR = 0;	/* Disable interrupts */
      *MCC_TIMER4_CR = 0;	/* and disable counter */
      status.running = FALSE;
      ALM_DBG(4, "alm_stop: counter stopped.");
   }

   ALM_DBG(5, "Leaving alm_stop.");
}


/*+**************************************************************************
 *
 * Function:	alm_int_handler
 *
 * Description:	Interrupt handler (gets called at counter overflow).
 *              The counter is disabled and a user routine is called (if it 
 *              was connected by alm_start).
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_int_handler (void)
{
   ts48_Stamp ts_now, ts_diff;

#ifdef ALM_DEBUG
   int verb = dbg_level;

   dbg_level = -1;
#endif

   *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR;	/* acknowledge timer interrupt */


   logMsg("\nINTERRUPT CONTEXT:\n");


   if (status.in_use) {		/* User call is active */

      status.woke_up = TRUE;
      alm_stop();

   } else {			/* No users around */

      do {
	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 sem_post(alarm_list->sem_p); /* Post the semaphore */


	 logMsg("INT: Alarm posted.\n");
	 

	 alm_discard(alarm_list);     /* Cancel the active alarm */

	 
	 logMsg("INT: Alarm discarded.\n");
	 

	 ts48_get_stamp(&ts_now);     /* Wake up the next one if due */
      } while ((alarm_list != NULL) &&
	       TS48_GE(ts_now, alarm_list->time_due));

      if (alarm_list == NULL) {    /* No more alarms */
	 alm_stop();


	 logMsg("INT: Alarm clock stopped.\n");


	 return;
      }

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up alarm for first in line */
      ts_diff = ts48_sub(&(alarm_list->time_due), &ts_now);
      alm_setup(ts_diff.low);


      logMsg("INT: Alarm set up (in %d us).\n", ts_diff.low);
      

   }

#ifdef ALM_DEBUG
   dbg_level = verb;
#endif


   logMsg("INTERRUPT CONTEXT END\n\n");
   

}



/*+**************************************************************************
 *
 * Function:	alm_init
 *
 * Description:	Initialize the alarm library.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Error code:
 *                           0  -  OK.
 *                          -1  -  Error.
 *
 **************************************************************************-*/

int
alm_init (void)
{
#ifdef LCAL_DEBUG
   if (INIT_LOCK(alm_dbg_lock)) { /* Init debug semaphore */
      return(-1);
   }
#endif

   if (INIT_LOCK(alm_lock)) {	/* Init mutex semaphore */
      return(-1);
   }
   ts48_init();

   return(0);
}


/*+**************************************************************************
 *
 * Function:	alm_start
 *
 * Description:	Setup an alarm. After the given time (in us) the user
 *              semaphore will be posted by the alarm clock.
 *
 * Arg(s) In:	delay       -  Alarm delay in us.
 *              user_sem_p  -  Semaphore to be posted.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	ID (handle) of alarm (is needed to cancel an alarm).
 *                NULL  -  Error.
 *
 **************************************************************************-*/

alm_ID
alm_start (
   unsigned long delay,
   sem_t* user_sem_p
   )
{
   register Alarm_Entry* last_p = NULL;
   register Alarm_Entry* act_p  = alarm_list;
   Alarm_Entry* new_p;
   ts48_Stamp   ts_now, ts_diff;
   unsigned long i;

   ALM_DBG(5, "Entering alm_start.");

   LOCK(alm_lock);
   status.in_use = TRUE;
				/* Is the free list empty? */
   if (free_list == NULL) {
      ALM_DBG(4, "alm_start: free list is empty.");
				/* Get a new chunk of nodes */
      free_list = (Alarm_Entry*) calloc(LIST_CHUNK_SIZE, sizeof(Alarm_Entry));
      if (free_list == NULL) {	/* calloc error */
	 status.in_use = FALSE;
	 UNLOCK(alm_lock);
	 ALM_DBG(5, "Leaving alm_start.");
	 return(NULL);
      }
				/* and initialize it */
      for (i = 0; i < (LIST_CHUNK_SIZE - 1); i++) {
	 free_list[i].next_p = &free_list[i+1];
	 SET_MAGIC(&free_list[i], Alarm_Entry);
      }
      SET_MAGIC(&free_list[i], Alarm_Entry);

      ALM_DBG(3, "alm_start: new list nodes allocated.");
   }

   ts48_get_stamp(&ts_now);
   ts48_add_us(&ts_now, delay);
   
   new_p = free_list;		/* Take first entry from free list */
   free_list = free_list->next_p;
   
   ASSERT_STRUC(new_p, Alarm_Entry);

   new_p->time_due = ts_now;	/* Fill it up */
   new_p->sem_p    = user_sem_p;
   new_p->next_p   = NULL;

   ALM_PRF(3,("alm_start: new alarm (%p) created: due at %d/%d.\n",
	      new_p, new_p->time_due.high, new_p->time_due.low));
   
				/* Sort it into alarm list */
   if (act_p == NULL) {		/* Empty alarm list */
      alarm_list = new_p;
      ALM_DBG(4, "alm_start: alarm list was empty - new alarm is first.");
   } else {

      do {			/* Sweep through alarm list */
	 if (TS48_LT(ts_now, act_p->time_due))
	    break;

	 last_p = act_p;	     /* Go to next element */
	 act_p  = act_p->next_p;

      } while (act_p != NULL);

      new_p->next_p = act_p;	     /* Insert new element */
      if (last_p == NULL)
	 alarm_list = new_p;
      else
	 last_p->next_p = new_p;

      ALM_PRF(4, ("alm_start: new alarm is before %p.\n", act_p));
   }

   if (status.woke_up) {	/* The timer interrupted */

      ALM_DBG(3, "alm_start: int handler was called.");

      do {
	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 sem_post(alarm_list->sem_p); /* Post the semaphore */
	 ALM_PRF(3, ("alm_start: alarm %p posted.\n", alarm_list));

	 alm_discard(alarm_list);     /* Cancel the active alarm */

	 ts48_get_stamp(&ts_now);     /* Wake up the next one if due */
      } while ((alarm_list != NULL) &&
	       TS48_GE(ts_now, alarm_list->time_due));

      if (alarm_list == NULL) {    /* No more alarms */
	 ALM_DBG(3, "alm_start: no more alarms.");

	 alm_stop();
	 status.in_use = FALSE;
	 UNLOCK(alm_lock);

	 ALM_DBG(5, "Leaving alm_start.");
	 return(NULL);
      }

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up alarm for first in line */
      ts_diff = ts48_sub(&(alarm_list->time_due), &ts_now);
      alm_setup(ts_diff.low);

      ALM_PRF(3, ("alm_start: next alarm (%p) set up (in %d us).\n",
		  alarm_list, ts_diff.low));

      status.woke_up = FALSE;

   } else if (alarm_list == new_p) { /* First list element changed */

      alm_stop();

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Wake up the next one if due */
      ts48_get_stamp(&ts_now);
      ts_diff = ts48_sub(&(alarm_list->time_due), &ts_now);
      alm_setup(ts_diff.low);

      ALM_PRF(3, ("alm_start: next alarm (%p) set up (in %d us).\n",
		  alarm_list, ts_diff.low));
   }

   status.in_use = FALSE;
   UNLOCK(alm_lock);

   ALM_DBG(5, "Leaving alm_start.");
   return(new_p);		/* Return alarm id */
}


/*+**************************************************************************
 *
 * Function:	alm_cancel
 *
 * Description:	Cancel an alarm.
 *
 * Arg(s) In:	id  -  Id of alarm to be cancelled.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void
alm_cancel (alm_ID id)
{
   register Alarm_Entry* act_p  = alarm_list;
   ts48_Stamp ts_now, ts_diff;

   ALM_DBG(5, "Entering alm_cancel.");

   if (!act_p) {
      ALM_DBG(3, "alm_cancel: Empty alarm list.");
      ALM_DBG(5, "Leaving alm_cancel.");
      return;		/* Return on empty alarm list */
   }
   
   if (act_p == id) {		/* Cancel the running alarm */
      alm_stop();
      ALM_PRF(3, ("alm_cancel: current alarm (%p) stopped.\n", id));
   }

   LOCK(alm_lock);
   status.in_use = TRUE;

   alm_discard(id);

   if (alarm_list != NULL) {	/* If there's another alarm left, */
				/* set up counter for new alarm */

      ASSERT_STRUC(alarm_list, Alarm_Entry);

      ts48_get_stamp(&ts_now);
      ts_diff = ts48_sub(&(alarm_list->time_due), &ts_now);

      alm_setup(ts_diff.low);
      ALM_PRF(3, ("alm_cancel: next alarm (%p) set up (in %d us).\n",
		  alarm_list, ts_diff.low));
   }

   status.in_use = FALSE;
   UNLOCK(alm_lock);
   
   ALM_DBG(5, "Leaving alm_cancel.");
}



/*+**************************************************************************
 *
 * Function:	alm_freq
 *
 * Description:	Returns the frequency of the alarm clock, in ticks per
 *              second.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	The alarm clock frequency, in ticks per second.
 *
 **************************************************************************-*/

UINT32 alm_freq (void)
{
   UINT32 timerFreq;
   UINT32 Bclk;

   ALM_DBG(5, "Entering alm_freq.");

   if (*MCC_VERSION_REG & VERSION_REG_SPEED)	/* check clock speed */
      Bclk = 33000000;
   else
      Bclk = 25000000;

   timerFreq = Bclk/(256 - (*MCC_PRESCALE_CLK_ADJ & 0xff));

   ALM_DBG(5, "Leaving alm_freq.");
   return (timerFreq);
}


#ifdef ALM_DEBUG
/*+**************************************************************************
 *
 * Function:	almSetDebug
 *
 * Description:	Sets verbosity level.
 *
 * Arg(s) In:	verb  -  New verbosity level (-1 = no messages)
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	0.
 *
 **************************************************************************-*/

int almSetDebug (char verb)
{
   dbg_level = verb;
   return(0);
}
#endif /* #ifdef ALM_DEBUG */
