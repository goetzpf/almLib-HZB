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
 * $Revision: 1.6 $
 * $Date: 1996/08/30 13:35:50 $
 *
 * $Author: lange $
 *
 * $Log: almLib.c,v $
 * Revision 1.6  1996/08/30 13:35:50  lange
 * More interrupt locking (against spurious interrupts).
 *
 * Revision 1.5  1996/06/06 14:54:50  lange
 * Timer is not reset at startup; debug info changes.
 *
 * Revision 1.4  1996/06/04 10:01:12  lange
 * Secure against multiple initialisation.
 *
 * Revision 1.3  1996/06/03 20:18:13  lange
 * Alarms seem to work now (multiple bugs fixed).
 *
 * Revision 1.2  1996/05/22 14:06:11  lange
 * New watchdog-like functionality.
 *
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
rcsid[] = "@(#)mCAN-timer: $Id: almLib.c,v 1.6 1996/08/30 13:35:50 lange Exp $";


#include <vxWorks.h>
#include <stdlib.h>
#include <intLib.h>
#include <iv.h>

#define MCC_BASE_ADRS   (0xfff42000)             /* MCchip reg base address */
#define MCC_INT_VEC_BASE       0x40     /* MCC interrupt vector base number */

#include <drv/multi/mcchip.h>

#include "alm.h"


#define INT_INHIBIT  25ul	/* Minimum interrupt delay */
#define INT_SUBTRACT 42ul	/* Subtracted from every delay */

#define ALARMCLOCK_LEVEL 1	/* Alarm clock's interrupt level */
#define LIST_CHUNK_SIZE 20	/* Number of elements in alarm list chunk */


/*+**************************************************************************
 *		DEBUG mode
 **************************************************************************-*/

#ifdef ALM_DEBUG

#include <semaphore.h>

extern void logMsg(char*, ...);

#define ALM_DBG(verb, string) {				\
   if (verb <= dbg_level) {				\
      if (!int_context) sem_wait(&alm_dbg_lock);	\
      logMsg(__FILE__ ":%d:\n", __LINE__);		\
      logMsg("%s\n", string);				\
      if (!int_context) sem_post(&alm_dbg_lock);	\
   }							\
}

#define ALM_PRF(verb, x) {				\
   if (verb <= dbg_level) {				\
      if (!int_context) sem_wait(&alm_dbg_lock);	\
      logMsg(__FILE__ ":%d:\n", __LINE__);		\
      logMsg x;						\
      if (!int_context) sem_post(&alm_dbg_lock);	\
   }							\
}

#define ASSERT_STRUC(str, type) {				\
   if ((str)->magic1 != type##_MAGIC1)				\
      if (1 <= dbg_level) {					\
	 if (!int_context) sem_wait(&alm_dbg_lock);		\
	 logMsg(__FILE__ ":%d: PANIC: " #type			\
		" structure head corrupt.\n", __LINE__);	\
	 if (!int_context) sem_post(&alm_dbg_lock);		\
      };							\
   if ((str)->magic2 != type##_MAGIC2)				\
      if (1 <= dbg_level) {					\
	 if (!int_context) sem_wait(&alm_dbg_lock);		\
	 logMsg(__FILE__ ":%d: PANIC: " #type			\
		" structure tail corrupt.\n", __LINE__);	\
	 if (!int_context) sem_post(&alm_dbg_lock);		\
      };							\
}

#define SET_MAGIC(str, type) {			\
   (str)->magic1 = type##_MAGIC1;		\
   (str)->magic2 = type##_MAGIC2;		\
}

   
#define Alarm_Entry_MAGIC1 0x12345601
#define Alarm_Entry_MAGIC2 0x12345602
#define MAGIC1 unsigned long magic1;
#define MAGIC2 unsigned long magic2;

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

#ifdef ALM_DEBUG

#define LOCK(name) {						\
   sem_wait(&name);						\
   if (4 <= dbg_level) {					\
      sem_wait(&alm_dbg_lock);					\
      logMsg(__FILE__ ":%d: " #name " locked.\n", __LINE__);	\
      sem_post(&alm_dbg_lock);					\
   }								\
}
#define UNLOCK(name) {						\
   sem_post(&name);						\
   if (4 <= dbg_level) {					\
      sem_wait(&alm_dbg_lock);					\
      logMsg(__FILE__ ":%d: " #name " unlocked.\n", __LINE__);	\
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
   MAGIC1
   unsigned long  time_due;	/* Alarm time */
   char           is_new;
   sem_t*         sem_p;	/* Semaphore to give */
   struct a_node* next_p;
   MAGIC2
} Alarm_Entry;


/*+**************************************************************************
 *		Globals
 **************************************************************************-*/

static struct 
{
   unsigned int init_d  : 1;	/* Initialized flag */
   unsigned int running : 1;	/* Counter running flag */
   unsigned int in_use  : 1;	/* Lists in use flag */
   unsigned int woke_up : 1;	/* Int handler was called flag */
   unsigned int entries : 8;	/* Used Alarm_Entries */
} status = {FALSE, FALSE, FALSE, FALSE, 0};

static unsigned long last_checked; /* Time of last alarm list check */

static Alarm_Entry* alarm_list;	/* List of pending alarms */
static Alarm_Entry* last_alarm;	/* End of alarm list */
static Alarm_Entry* free_list;	/* List of unused alarm entries */

DECLARE_LOCK(alm_lock);		/* Mutex semaphore */

#ifdef ALM_DEBUG
DECLARE_LOCK(alm_dbg_lock);	/* Declare debug semaphore */
static char dbg_level   = -1;	/* Debug level (default=none) */
static char int_context = FALSE; /* Flag for interrupt context */
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

/* Check for and post alarms due */
static void alm_check (void);

/* The alarm counter's interrupt handler */
static void alm_int_handler (int arg);



/*+**************************************************************************
 *
 * Function:	alm_get_stamp
 *
 * Description:	Returns a time stamp from the alarm timer (32 bit;
 *              1 us resolution)
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Time stamp value.
 *
 **************************************************************************-*/

unsigned long alm_get_stamp (void)
{
   return(*MCC_TIMER4_CNT);
}


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
   register Alarm_Entry* last_p = last_alarm;
   register Alarm_Entry* act_p  = alarm_list;
   char found = FALSE;

   ALM_DBG(5, "Entering alm_discard.");

   if (act_p == NULL) {
      ALM_DBG(3, "alm_discard: alarm list empty.");
      return;	/* Return on empty alarm list */
   }

   do {
      if (act_p == id) {
	 found = TRUE;
	 break;
      }
      
      last_p = act_p;		/* Go to next element */
      act_p  = act_p->next_p;
      
   } while (act_p != alarm_list);

   if (!found) {
      ALM_DBG(3, "alm_discard: specified alarm not found.");
      return;	/* Alarm not found */
   }
   
   ASSERT_STRUC(act_p, Alarm_Entry);

   if (act_p == last_p) {	/* Only one alarm left */
      ALM_PRF(3, ("alm_discard: alarm list is now empty.\n"));
      last_alarm = NULL;
      alarm_list = NULL;
   } else {			/* Take alarm out of alarm list */
      last_p->next_p = act_p->next_p;
      if (act_p == alarm_list) alarm_list = act_p->next_p;
   }
   
   act_p->next_p = free_list;	/* Put unused entry into free list */
   free_list = act_p;

   ALM_PRF(3, ("alm_discard: alarm entry %p moved to free list.\n",
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
   register int lock_key;
   
   ALM_DBG(5, "Entering alm_setup.");

   status.running = TRUE;	/* Set running flag */

   lock_key = intLock();
				/* Set the timer compare value */
   *MCC_TIMER4_CMP = *MCC_TIMER4_CNT + 
      (delay < INT_INHIBIT ? INT_INHIBIT : delay);

				/* Reset & enable the alarm timer interrupt */
   *MCC_T4_IRQ_CR  = T4_IRQ_CR_IEN | T4_IRQ_CR_ICLR | ALARMCLOCK_LEVEL;

   intUnlock(lock_key);
   
   ALM_PRF(4, ("alm_setup: counter set to %u.\n", delay));
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
   register int lock_key;

   ALM_DBG(5, "Entering alm_stop.");

   if (status.running) {

      lock_key = intLock();
      *MCC_T4_IRQ_CR = 0;	/* Disable interrupts */
      intUnlock(lock_key);

      status.running = FALSE;
      ALM_DBG(4, "alm_stop: counter stopped.");
   }

   ALM_DBG(5, "Leaving alm_stop.");
}



/*+**************************************************************************
 *
 * Function:	alm_check
 *
 * Description:	Checks the alarm list for alarms due and posts them.
 * 		Sets up the timer for next pending alarm.
 *              Mutex must be locked when using this function.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_check (void)
{
   register unsigned long ts;
   char found;


   do {
      found = FALSE;
      ts = alm_get_stamp() - last_checked;

				/* Check for alarms due */
      while ((alarm_list != NULL) &&
	     (ts >= alarm_list->time_due - last_checked)) {

	 found = TRUE;

	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 if (alarm_list->is_new) { /* This one is for the next round */
	    alarm_list->is_new = FALSE;
	    alarm_list = alarm_list->next_p;
	    ALM_PRF(3, ("alm_check: alarm %p ignored.\n", alarm_list));
	 } else {
	    sem_post(alarm_list->sem_p); /* Post the semaphore */
	    ALM_PRF(2, ("alm_check: alarm %p posted.\n", alarm_list));
	    
	    alm_discard(alarm_list);	/* Cancel the active alarm */
	 }
      }
      last_checked += ts;
   } while (found);

   if (alarm_list == NULL) {    /* No more alarms */

      alm_stop(); 
      ALM_DBG(3, "alm_check: no more alarms, timer stopped.");

   } else {

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up alarm for first in line */
      ts = alarm_list->time_due - last_checked;
      alm_setup(ts);

      ALM_PRF(3, ("alm_check: timer set up for next alarm (%p) in %u us.\n",
		  alarm_list, ts));
   }
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

void alm_int_handler (int arg)
{
   register int lock_key;

#ifdef ALM_DEBUG
   int_context = TRUE;
#endif

   lock_key = intLock();
   *MCC_T4_IRQ_CR |= T4_IRQ_CR_ICLR; 	/* acknowledge timer interrupt */
   intUnlock(lock_key);

   if (status.in_use) {		/* User call is active */

      status.woke_up = TRUE;
      ALM_DBG(3, "Handler woke up.");

   } else {			/* No users around */

      alm_check();		/* Check for and post alarms */

   }

#ifdef ALM_DEBUG
   int_context = FALSE;
#endif
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
   ALM_DBG(5, "Entering alm_init.");

   if (!status.init_d) {
      status.init_d = TRUE;

#ifdef ALM_DEBUG
      if (INIT_LOCK(alm_dbg_lock)) { /* Init debug semaphore */
	 return(-1);
      }
#endif
      if (INIT_LOCK(alm_lock)) { /* Init mutex semaphore */
	 return(-1);
      }
				/* Connect interrupt handler */
      (void) intConnect (INUM_TO_IVEC (MCC_INT_VEC_BASE + MCC_INT_TT4),
			 alm_int_handler, NULL);

				/* Enable the timer */
      *MCC_TIMER4_CR  = TIMER4_CR_CEN;
   
      ALM_DBG(1, "alm: initialization done.");
   }

   ALM_DBG(5, "Leaving alm_init.");
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
   register Alarm_Entry* last_p;
   register Alarm_Entry* act_p;
   Alarm_Entry* new_p;
   unsigned long ts_due, ts_now, ts_ref;
   unsigned long i;
   char moved;

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
      status.entries += LIST_CHUNK_SIZE;

      for (i = 0; i < (LIST_CHUNK_SIZE - 1); i++) {
	 free_list[i].next_p = &free_list[i+1];
	 SET_MAGIC(&free_list[i], Alarm_Entry);
      }

      free_list[LIST_CHUNK_SIZE - 1].next_p = NULL;
      SET_MAGIC(&free_list[LIST_CHUNK_SIZE - 1], Alarm_Entry);

      ALM_DBG(3, "alm_start: new list nodes allocated.");
   }


   new_p = free_list;		/* Take first entry from free list */
   free_list = free_list->next_p;
   
   ASSERT_STRUC(new_p, Alarm_Entry);

   ts_now = alm_get_stamp();	/* Calculate time due */
   ts_due = ts_now + (delay > INT_SUBTRACT ? (delay - INT_SUBTRACT) : 0);

   new_p->time_due = ts_due;	/* Fill the alarm entry */
   new_p->sem_p    = user_sem_p;
   new_p->next_p   = NULL;
   if ((ts_due - last_checked <  ts_now - last_checked) &&
       (ts_due - last_checked >= alarm_list->time_due - last_checked))
      new_p->is_new = TRUE;
   else
      new_p->is_new = FALSE;

   ALM_PRF(2,("alm_start: new alarm (%p) created: due at %u.\n",
	      new_p, new_p->time_due));
   
				/* Sort it into alarm list */
   last_p = last_alarm;
   act_p  = alarm_list;

   if (act_p == NULL) {		/* Empty alarm list */
      alarm_list    = new_p;	/* Whow */
      new_p->next_p = new_p;
      last_alarm    = new_p;
      ALM_DBG(4, "alm_start: alarm list was empty - new alarm is first.");

   } else {

      /* Sorting into alarm list is done with relative delays;
	 reference is now or alarm_list->time_due (whichever is older) */
      if (ts_now - last_checked > alarm_list->time_due - last_checked)
	 ts_ref = alarm_list->time_due;
      else
	 ts_ref = ts_now;
      
      ts_due  -= ts_ref;	/* Make time due relative */

      moved = FALSE;
				/* Sweep through alarm list */
      while (ts_due > (act_p->time_due - ts_ref)) {
	 moved  = TRUE;
	 last_p = act_p;	     /* Go to next element */
	 act_p  = act_p->next_p;
	 if (act_p == alarm_list) break;
      }

      new_p->next_p  = act_p;	     /* Insert new element */
      last_p->next_p = new_p;
				     /* Not moved? This is new head */
      if (!moved) {
	 alarm_list = new_p;
	 ALM_PRF(4, ("alm_start: new alarm is first (before %p).\n", act_p));
      } else if (act_p == alarm_list) {
	 last_alarm = new_p;
	 ALM_PRF(4, ("alm_start: new alarm is last (after %p).\n", last_p));
      } else {
	 ALM_PRF(4, ("alm_start: new alarm is between %p and %p.\n",
		     last_p, act_p));
      }
   }

   if (alarm_list == new_p) { /* First list element changed */

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up the next alarm */
      ts_due = alarm_list->time_due - ts_now;
      alm_setup(ts_due);

      ALM_PRF(3, ("alm_start: next alarm (%p) set up (in %u us).\n",
		  alarm_list, ts_due));
   }

   do {				/* The timer interrupted */
      status.in_use  = TRUE;
      status.woke_up = FALSE;

      ALM_DBG(3, "alm_start: int handler was called.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

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
   register Alarm_Entry* act_p;
   unsigned long ts_diff;

   ALM_DBG(5, "Entering alm_cancel.");

   LOCK(alm_lock);
   status.in_use = TRUE;

   act_p  = alarm_list;

   if (!act_p) {
      ALM_DBG(3, "alm_cancel: Empty alarm list.");
      ALM_DBG(5, "Leaving alm_cancel.");
      return;		/* Return on empty alarm list */
   }
   
   if (act_p == id) {		/* Cancel the running alarm */
      alm_stop();
      ALM_PRF(3, ("alm_cancel: current alarm (%p) stopped.\n", id));

      alm_discard(id);		/* Discard it */

      if (alarm_list != NULL) {	/* If there's another alarm left, */
				/* set up counter for new alarm */

	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 ts_diff = alarm_list->time_due - alm_get_stamp();

	 alm_setup(ts_diff);
	 ALM_PRF(3, ("alm_cancel: next alarm (%p) set up (in %d us).\n",
		     alarm_list, ts_diff));
      }

   } else {			/* Not the running alarm */

      alm_discard(id);		/* Discard the alarm */

   }

   do {				/* The timer interrupted */
      status.in_use  = TRUE;
      status.woke_up = FALSE;

      ALM_DBG(3, "alm_cancel: int handler was called.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

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



/*+**************************************************************************
 *
 * Function:	almInfo
 *
 * Description:	Prints debug info about alarms.
 *
 * Arg(s) In:	verb  -  Verbosity level:
 *                         0 = short
 *                         1 = long
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None
 *
 **************************************************************************-*/

void almInfo (unsigned char verb)
{
   register Alarm_Entry* act_p;
   unsigned short i = 0;

   printf("Alarm Info\n"
	  "----------\n"
	  "Last check was at %u; time now is %u; alarm ",
	  last_checked,
	  alm_get_stamp()
      );

   if (status.running)
      printf("is ");
   else
      printf("is NOT ");
   
   printf("RUNNING;\npending Alarms are:\n"
      );
   
   LOCK(alm_lock);
   status.in_use = TRUE;

   act_p  = alarm_list;

   if (act_p == NULL)
      printf("NO ALARMS.\n");
   else
      do {
	 i++;

	 if ((i-1) % 25 == 0)
	    printf("No N Time due   AlarmID  Semaphore\n"
		   "----------------------------------\n");

	 printf("%2d ", i);

	 if (act_p->is_new)
	    printf("N ");
	 else
	    printf("  ");
	 
	 printf("%10u %p %p\n",
		act_p->time_due,
		act_p,
		act_p->sem_p
	    );
	 act_p = act_p->next_p;
      } while (act_p != alarm_list);
   
   printf("%d used out of %d allocated alarm entries (%d bytes).\n",
	  i,
	  status.entries,
	  status.entries * sizeof(Alarm_Entry)
      );

   do {				/* The timer interrupted */
      status.in_use  = TRUE;
      status.woke_up = FALSE;

      ALM_DBG(3, "almInfo: int handler was called.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

   UNLOCK(alm_lock);
}
#endif /* #ifdef ALM_DEBUG */
