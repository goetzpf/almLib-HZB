/* $RCSfile: almLib.c,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		Contains the hardware inpependent parts of this library.
 *		The appropriate lower level driver code is included.
 *		the target architecture should be cpp-defined:
 *		mv162    -  for Motorola's MVME162 board family
 *		vmod60   -  Janz VMOD-60 cpu
 *		eltec27  -  Eltec Eurocom E27
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 1.8 $
 * $Date: 1997/02/07 16:04:33 $
 *
 * $Author: lange $
 *
 * $Log: almLib.c,v $
 * Revision 1.8  1997/02/07 16:04:33  lange
 * Added counter increment; made alm a module of its own.
 *
 * Revision 1.7  1996/10/29 13:11:53  lange
 * First version to go into EPICS tree (locally).
 *
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
 * Copyright (c) 1996, 1997
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/

static char
rcsid[] = "@(#)almLib: $Id: almLib.c,v 1.8 1997/02/07 16:04:33 lange Exp $";


#include <vxWorks.h>
#include <stdio.h>
#include <stdlib.h>

#include <debugmsg.h>
#include <alm.h>

#define LIST_CHUNK_SIZE 20	/* Number of elements in alarm list chunk */

#define Alarm_Entry_MAGIC1 0x12345601
#define Alarm_Entry_MAGIC2 0x12345602


/*+**************************************************************************
 *		Types
 **************************************************************************-*/

typedef struct a_node
{
   MAGIC1
   unsigned long  time_due;	/* Alarm time */
   char           is_new;
   sem_t*         sem_p;	/* Semaphore to give */
   unsigned long* cnt_p;	/* Counter to increment */
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

DECLARE_LOCK(alm_lock);		/* Mutex semaphore */
DBG_DECLARE			/* Debug Messages */

/*+**************************************************************************
 *		Local Functions
 **************************************************************************-*/

/* Discard an alarm (alarm list -> free list) */
static void alm_discard (Alarm_Entry* id);

/* Check for and post alarms due */
static void alm_check (void);

DBG_IMPLEMENT(alm)		/* Debug stuff */

				/* MOTOROLA MV 162 */
#if defined(mv162)

#define MCC_BASE_ADRS   (0xfff42000)   /* MCchip reg base address */
#define MCC_INT_VEC_BASE       0x40    /* MCC interrupt vector base number */
#define ALARMCLOCK_LEVEL          1    /* Alarm clock's interrupt level */

#define INT_INHIBIT  25ul	/* Minimum interrupt delay */
#define INT_SUBTRACT 42ul	/* Subtracted from every delay */

#include "alm_mcc.c"

				/* JANZ VMOD-60 */
#elif defined(vmod60)

#define	CIO1_BASE_ADRS    (0xfec30000)	/* SYSTEM CIO */
#define	CIO2_BASE_ADRS    (0xfec10000)	/* USER CIO */
#define ZCIO1_C           (unsigned char*)(CIO1_BASE_ADRS+0)
#define ZCIO2_CNTRL_ADRS  (unsigned char*)(CIO2_BASE_ADRS+3)

#define	ZCIO_HZ		  (5000000/2)	/* clocks per second */
#define INT_VEC_BASE_CIO2 0xa0	/* Interrupt vector base */
#define INT_VEC_CT2_3     (INT_VEC_BASE_CIO2 + 0)
#define INT_VEC_CT2_2     (INT_VEC_BASE_CIO2 + 2)
#define INT_VEC_CT2_1     (INT_VEC_BASE_CIO2 + 4)
#define INT_VEC_CT2_ERR   (INT_VEC_BASE_CIO2 + 6)

#define ZERO 0

#define INT_INHIBIT  50ul	/* Minimum interrupt delay */
#define INT_SUBTRACT 0ul	/* Subtracted from every delay */

#include "alm_z8536.c"

				/* ELTEC E27 */
#elif defined(eltec27)

#define	CIO1_BASE_ADRS    (0xfec30000)	/* SYSTEM CIO */
#define	CIO2_BASE_ADRS    (0xfec10000)	/* USER CIO */
#define ZCIO1_C           (unsigned char*)(CIO1_BASE_ADRS+0)
#define ZCIO2_CNTRL_ADRS  (unsigned char*)(CIO2_BASE_ADRS+3)

#define	ZCIO_HZ		  (5000000/2)	/* clocks per second */
#define INT_VEC_BASE_CIO2 0xa0	/* Interrupt vector base */
#define INT_VEC_CT2_3     (INT_VEC_BASE_CIO2 + 0)
#define INT_VEC_CT2_2     (INT_VEC_BASE_CIO2 + 2)
#define INT_VEC_CT2_1     (INT_VEC_BASE_CIO2 + 4)
#define INT_VEC_CT2_ERR   (INT_VEC_BASE_CIO2 + 6)

#define ZERO 0

#define INT_INHIBIT  50ul	/* Minimum interrupt delay */
#define INT_SUBTRACT 0ul	/* Subtracted from every delay */

#include "alm_z8536.c"

#endif

/*+**************************************************************************
 *		More Globals
 **************************************************************************-*/

static unsigned long last_checked; /* Time of last alarm list check */

static Alarm_Entry* alarm_list;	/* List of pending alarms */
static Alarm_Entry* last_alarm;	/* End of alarm list */
static Alarm_Entry* free_list;	/* List of unused alarm entries */



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

   DBG(5, "Entering alm_discard.");

   if (act_p == NULL) {
      DBG(3, "alm_discard: alarm list empty.");
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
      DBG(3, "alm_discard: specified alarm not found.");
      return;	/* Alarm not found */
   }
   
   ASSERT_STRUC(act_p, Alarm_Entry);

   if (act_p == last_p) {	/* Only one alarm left */
      DBG(3, "alm_discard: alarm list is now empty.");
      last_alarm = NULL;
      alarm_list = NULL;
   } else {			/* Take alarm out of alarm list */
      last_p->next_p = act_p->next_p;
      if (act_p == alarm_list) alarm_list = act_p->next_p;
   }
   
   act_p->next_p = free_list;	/* Put unused entry into free list */
   free_list = act_p;

   PRF(3, ("alm_discard: alarm entry %p moved to free list.\n",
	       act_p));
   DBG(5, "Leaving alm_discard.");
}


/*+**************************************************************************
 *
 * Function:	alm_check
 *
 * Description:	Checks the alarm list for alarms due and posts semaphores /
 *		increments counters.
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
   register char found;

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
	    PRF(3, ("alm_check: alarm %p ignored.\n", alarm_list));
	 } else {
	    sem_post(alarm_list->sem_p); /* Post the semaphore */
	    if (alarm_list->cnt_p)       /* Increment the counter */
	       (*(alarm_list->cnt_p))++;
	    PRF(2, ("alm_check: alarm %p posted.\n", alarm_list));
	    
	    alm_discard(alarm_list);	/* Cancel the active alarm */
	 }
      }
      last_checked += ts;
   } while (found);

   if (alarm_list == NULL) {    /* No more alarms */

      alm_disable(); 
      DBG(3, "alm_check: no more alarms, timer disabled.");

   } else {

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up alarm for first in line */
      ts = alarm_list->time_due - last_checked;
      alm_setup(ts);

      PRF(3, ("alm_check: timer set to next alarm (%p) in %lu us.\n",
		  alarm_list, ts));
   }
}


/*+**************************************************************************
 *
 * Function:	alm_start
 *
 * Description:	Setup an alarm. After the given time (in us) the user
 *              semaphore will be posted by the alarm clock.
 *              Must not be called from interrupt context.
 *
 * Arg(s) In:	delay  -  Alarm delay in us.
 *              sem_p  -  Semaphore to be posted.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	ID (handle) of alarm (is needed to cancel an alarm).
 *                NULL  -  Error.
 *
 **************************************************************************-*/

alm_ID
alm_start (
   unsigned long  delay,
   sem_t*         sem_p,
   unsigned long* cnt_p
   )
{
   register Alarm_Entry* last_p;
   register Alarm_Entry* act_p;
   Alarm_Entry* new_p;
   unsigned long ts_due, ts_now, ts_ref;
   unsigned long i;
   char moved;

   DBG(5, "Entering alm_start.");

   LOCK(alm_lock);
   status.in_use = TRUE;

				/* Is the free list empty? */
   if (free_list == NULL) {
      DBG(4, "alm_start: free list is empty.");
				/* Get a new chunk of nodes */
      free_list = (Alarm_Entry*) calloc(LIST_CHUNK_SIZE, sizeof(Alarm_Entry));

      if (free_list == NULL) {	/* calloc error */
	 status.in_use = FALSE;
	 UNLOCK(alm_lock);
	 DBG(5, "Leaving alm_start.");
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

      DBG(3, "alm_start: new list nodes allocated.");
   }

   new_p = free_list;		/* Take first entry from free list */
   free_list = free_list->next_p;
   
   ASSERT_STRUC(new_p, Alarm_Entry);

   ts_now = alm_get_stamp();	/* Calculate time due */
   ts_due = ts_now + (delay > INT_SUBTRACT ? (delay - INT_SUBTRACT) : 0);

   new_p->time_due = ts_due;	/* Fill the alarm entry */
   new_p->sem_p    = sem_p;
   new_p->cnt_p    = cnt_p;
   new_p->next_p   = NULL;
   if ((ts_due - last_checked <  ts_now - last_checked) &&
       (ts_due - last_checked >= alarm_list->time_due - last_checked))
      new_p->is_new = TRUE;
   else
      new_p->is_new = FALSE;

   PRF(2,("alm_start: new alarm (%p) created: due at %u.\n",
	      new_p, new_p->time_due));
   
				/* Sort it into alarm list */
   last_p = last_alarm;
   act_p  = alarm_list;

   if (act_p == NULL) {		/* Empty alarm list */
      alarm_list    = new_p;	/* Whow */
      new_p->next_p = new_p;
      last_alarm    = new_p;
      DBG(4, "alm_start: alarm list was empty - new alarm is first.");

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
	 moved = TRUE;
	 act_p = (last_p = act_p)->next_p; /* Go to next element */
	 if (act_p == alarm_list) break;
      }

      new_p->next_p  = act_p;	     /* Insert new element */
      last_p->next_p = new_p;
				     /* Not moved? This is new head */
      if (!moved) {
	 alarm_list = new_p;
	 PRF(4, ("alm_start: new alarm is first (before %p).\n", act_p));
      } else if (act_p == alarm_list) {
	 last_alarm = new_p;
	 PRF(4, ("alm_start: new alarm is last (after %p).\n", last_p));
      } else {
	 PRF(4, ("alm_start: new alarm is between %p and %p.\n",
		     last_p, act_p));
      }
   }

   if (alarm_list == new_p) { /* First list element changed */

      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up the next alarm */
      ts_due = alarm_list->time_due - ts_now;
      alm_setup(ts_due);

      PRF(3, ("alm_start: next alarm (%p) set up (in %u us).\n",
		  alarm_list, ts_due));
   }

   do {				/* The timer interrupted */
      status.in_use  = TRUE;
      status.woke_up = FALSE;

      DBG(3, "alm_start: checking for alarms due.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

   UNLOCK(alm_lock);

   DBG(5, "Leaving alm_start.");
   DBG(3, "==============================================================");

   return(new_p);		/* Return alarm id */
}


/*+**************************************************************************
 *
 * Function:	alm_cancel
 *
 * Description:	Cancel an alarm.
 *              Must not be called from interrupt context.
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

   DBG(5, "Entering alm_cancel.");

   LOCK(alm_lock);
   status.in_use = TRUE;

   act_p  = alarm_list;

   if (!act_p) {
      DBG(3, "alm_cancel: Empty alarm list.");
      status.in_use  = FALSE;
      UNLOCK(alm_lock);
      DBG(5, "Leaving alm_cancel.");
      return;		/* Return on empty alarm list */
   }
   
   if (act_p == id) {		/* Cancel the running alarm */
      alm_disable();
      PRF(3, ("alm_cancel: current alarm (%p) stopped.\n", id));

      alm_discard(id);		/* Discard it */

      if (alarm_list != NULL) {	/* If there's another alarm left, */
				/* set up counter for new alarm */

	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 ts_diff = alarm_list->time_due - alm_get_stamp();

	 alm_setup(ts_diff);
	 PRF(3, ("alm_cancel: next alarm (%p) set up (in %d us).\n",
		     alarm_list, ts_diff));
      }

   } else {			/* Not the running alarm */

      alm_discard(id);		/* Discard the alarm */

   }

   do {				/* The timer interrupted */
      status.in_use  = TRUE;
      status.woke_up = FALSE;

      DBG(3, "alm_cancel: int handler was called.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

   UNLOCK(alm_lock);
   DBG(5, "Leaving alm_cancel.");
}


/*+**************************************************************************
 *
 * Function:	almShow
 *
 * Description:	Prints debug info about alarms.
 *              Must not be called from interrupt context.
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

void almShow (unsigned char verb)
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
	    printf("No N Time due   AlarmID  Semaphore  Counter\n"
		   "-------------------------------------------\n");

	 printf("%2d ", i);

	 if (act_p->is_new)
	    printf("N ");
	 else
	    printf("  ");
	 
	 printf("%10u %p %p ",
		act_p->time_due,
		act_p,
		act_p->sem_p
	    );

	 if (act_p->cnt_p)
	    printf("%8x\n", *(act_p->cnt_p));
	 else
	    printf(" ------\n");

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

      DBG(3, "almInfo: checking for alarms.");
      alm_check();		/* Check for and post alarms due */

      status.in_use  = FALSE;
   } while (status.woke_up);

   UNLOCK(alm_lock);
}
