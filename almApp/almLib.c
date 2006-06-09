/* $RCSfile: almLib.c,v $ */

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	almLib.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		Contains the hardware inpependent parts of this library.
 *		The appropriate lower level driver code is included.
 *		the target architecture should be cpp-defined:
 *		mv162    -  for Motorola's MVME162 board family
 *		vmod60   -  Janz VMOD-60 cpu
 *		eltec27  -  Eltec Eurocom E27
 *
 * To Do:
 *		- Should auto-configure at almInit() instead of setting
 *		  fixed values after querying the cpu speed.
 *              - Should use VME2chip to work on mv167
 *
 * Author(s):	Ralph Lange
 *
 * Revision log at end of file
 *
 * This software is copyrighted by the BERLINER SPEICHERRING-
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY.
 * The following terms apply to all files associated with the software.
 *
 * BESSY hereby grants permission to use, copy and modify this software
 * and its documentation for non-commercial, educational or research
 * purposes, provided that existing copyright notices are retained in
 * all copies.
 *
 * The receiver of the software provides BESSY with all enhancements,
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE
 * OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN
 * IF BESSY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NON-INFRINGEMENT. THIS SOFTWARE IS PROVIDED
 * ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS OR MODIFICATIONS.
 *
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/

static char
rcsid[] = "@(#)almLib: $Id: almLib.c,v 2.9.2.2 2006/06/09 09:54:09 franksen Exp $";


#include <vxWorks.h>          /* OK, ERROR                         */
#include <stdio.h>
#include <stdlib.h>
#include <iv.h>               /* for INUM_TO_VEC() macro           */
#include <intLib.h>           /* for intLock(), intUnlock()        */
#include <semaphore.h>        /* for sem_t                         */
#include <string.h>           /* for memmove()                     */
#include <debugmsg.h>         /* DBG_DECLARE,DBG_MSG,etc macros    */
#include <logLib.h>           /* logMsg() prototype                */
#include <assert.h>

#include "alm.h"              /* for alm_init_symTbl() prototype  */
#include "almLib.h"           /* alm_check() prototype            */



/*+**************************************************************************
 *              Types
 **************************************************************************-*/
typedef struct a_node
{
   MAGIC1
   unsigned long  time_due;     /* Alarm time */
   char           is_new;
   sem_t*         sem_p;        /* Semaphore to give */
   unsigned long* cnt_p;        /* Counter to increment */
   struct a_node* next_p;
   MAGIC2
} Alarm_Entry;

/*+**************************************************************************
 *		Local variables
 **************************************************************************-*/

DECLARE_LOCK(alm_lock);                         /* Mutex semaphore               */
DBG_DECLARE                                     /* Debug Messages                */

static unsigned long    int_subtract=0;         /* interrupt delay adjutsment    */
static unsigned long    last_checked=0;         /* Time of last alarm list check */
static Alarm_Entry     *alarm_list     = NULL;  /* List of pending alarms        */
static Alarm_Entry     *last_alarm     = NULL;  /* End of alarm list             */
static Alarm_Entry     *free_list      = NULL;  /* List of unused alarm entries  */
static alm_status_ts   *alm_status_ps  = NULL;  /* alarm status bitfield         */
static alm_func_tbl_ts *alm_ps         = NULL;  /* alarm timer func symbol tbl   */ 

int almLibIsPrimitive = TRUE;

 /*+**************************************************************************
 *		Parameter Definitions
 **************************************************************************-*/

#define INT_SUBTRACT        int_subtract
#define LIST_CHUNK_SIZE     20        /* No of elements in alarm list chunk */
#define Alarm_Entry_MAGIC1  0x12345601
#define Alarm_Entry_MAGIC2  0x12345602

/*+**************************************************************************
 *		Local Functions Prototypes 
 **************************************************************************-*/

/* Discard an alarm list entry */
static void alm_discard (Alarm_Entry* id);

DBG_IMPLEMENT(alm)	               	        /* Debug stuff */



/*+**************************************************************************
 *
 * Function:	almInit
 *
 * Description:	Initialize the alarm library. This function MUST be called
 *              prior to all other almLib functons.
 *
 * Arg(s) In:	None.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Error code:
 *               OK                        - Successful completion
 *               ERROR                     - Failured to initialize the low-level almarm symbol table
 *               S_dev_vectorInUse         - Failure due to vector in use
 *               S_dev_vxWorksVecInstlFail - Failure due to intConnect() problem
 *
 **************************************************************************-*/

long almInit (void)
{
   long                    status = OK;  
   unsigned long           vector=0;
   static alm_func_tbl_ts *tbl_ps = NULL;


   DBG(5, "Entering almInit.");
   
   /* Has initialization begun? */
   if ( !alm_ps ) {
     tbl_ps = alm_init_symTbl();
     if ( !tbl_ps ) 
        status = ERROR;
     else if ( !tbl_ps->getStatus ) {
        status = ERROR;
        logMsg("almInit: Failed to find low-level ALM_getStatus() routine.\n",0,0,0,0,0,0);
     } else { 
        alm_status_ps = (*tbl_ps->getStatus)();
        alm_ps = tbl_ps;
     }  
     /* Check for errors before continuing */
     if ( status!=OK ) {
        logMsg("almInit: Failed to initialize low-level alarm symbol table.\n",0,0,0,0,0,0);
        DBG(5, "Leaving almInit.");
        return(status);
     }  
     DBG(5,"almInit: Successfully initialize low-level alarm symbol table."); 
   }
   
   /* Is the timer alarm in progress? */
   if ( !alm_status_ps->init_s ) {
      alm_status_ps->init_s = TRUE;  
      DBG_INIT;
      
      /* Init mutex semaphore */
      if (INIT_LOCK(alm_lock)) { 
        logMsg("almInit: Function sem_init() failed.",0,0,0,0,0,0);
        status=ERROR;
      } else { 

        /* Attach isr to interrupt vector address */
        vector = alm_status_ps->int_vec;
	status = intConnect(INUM_TO_IVEC(vector),alm_int_handler,0);
        if (status==OK) {
          int_subtract = (*alm_ps->int_delay_adjust)();
	  
          /* Enable the timer. For the mcc chip (on mv162)
           * this means that the timer enable bit is set, but
           * the compare register must be set with a value before
           * and interrupt will occur. However, for the decrement
           * counter on the powerPC the priority and interrupt level
           * are set in the timer vector/priority register, however
           * to get interrupts the count inhibit bit in the timer
           * base count register must be cleared. So the sequence
           * of events here are quite important.
           */
          status = (*alm_ps->enable)();  
          if (status==OK) {   
            alm_status_ps->init_d = TRUE; 
            DBG(1, "almInit: Initialization completed successfully."); 
          } else  /* Done enabling timer */ 
             logMsg("almInit: Failed to enable alarm.",0,0,0,0,0,0);
        }/* Done connecting int handler to vector adrs */
	else
	  logMsg("almInit: Failed due to intConnect() error\n",0,0,0,0,0,0);
      } /* Done initializing mutex semaphone */  
   }/* Initialization complete */
   else if (alm_status_ps->init_d)
      DBG(1,"almInit: Initialization already performed successfully.");
   
   DBG(5, "Leaving almInit.");
   return (status); 
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

static void alm_discard (Alarm_Entry* id)
{
   register Alarm_Entry* last_p = last_alarm;
   register Alarm_Entry* act_p  = alarm_list;
   char                  found = FALSE;
   

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
   

   if ( !alm_ps || !alm_status_ps ) {
     logMsg("alm_check: low-level alarm symbol table has not been initizlized. Call almInit().\n",
             0,0,0,0,0,0);
     return;
  }

   do {
      found = FALSE;
      ts = (*alm_ps->get_stamp)() - last_checked;

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
      (*alm_ps->disable)();
      DBG(3, "alm_check: no more alarms, timer disabled.");

   } else {
      ASSERT_STRUC(alarm_list, Alarm_Entry);

				/* Set up alarm for first in line */
      ts = alarm_list->time_due - last_checked;
      (*alm_ps->setup)(ts);

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
 *              cnt_p  -  ??????
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	ID (handle) of alarm (is needed to cancel an alarm).
 *                NULL  -  Error.
 *
 **************************************************************************-*/

static sem_t *wake_up_sem;

alm_ID
alm_start (
   unsigned long  delay,
   sem_t*         sem_p,
   unsigned long* cnt_p
   )
{
   if (almLibIsPrimitive) {
      assert(alm_ps);
      assert(sem_p);
      wake_up_sem = sem_p;
      alm_ps->setup(delay);
      return 0;
   } else {
      register int          lock_key=0;
      register Alarm_Entry* last_p=NULL;
      register Alarm_Entry* act_p=NULL;
      Alarm_Entry*          new_p=NULL;
      unsigned long         ts_due=0;
      unsigned long         ts_now=0;
      unsigned long         ts_ref=0;
      unsigned long         i=0;
      char                  moved=FALSE;

      DBG(5, "Entering alm_start.");

      if ( !alm_ps || !alm_status_ps ) {
        logMsg("alm_start: low-level alarm symbol table has not been initizlized. Call almInit().\n",
                0,0,0,0,0,0);
        DBG(5, "Leaving alm_start.");     
        return(new_p);
     }

      LOCK(alm_lock);

      lock_key = intLock();
      alm_status_ps->in_use = TRUE;
      intUnlock(lock_key);
				   /* Is the free list empty? */
      if (free_list == NULL) {
         DBG(4, "alm_start: free list is empty.");
				   /* Get a new chunk of nodes */
         free_list = (Alarm_Entry*)
	    calloc(LIST_CHUNK_SIZE, sizeof(Alarm_Entry));

         if (free_list == NULL) {	/* calloc error */
            lock_key = intLock();
	    alm_status_ps->in_use = FALSE;
            intUnlock(lock_key);
	    UNLOCK(alm_lock);
	    DBG(5, "Leaving alm_start.");
	    return(NULL);
         }
				   /* and initialize it */
         lock_key = intLock();
         alm_status_ps->entries += LIST_CHUNK_SIZE;
         intUnlock(lock_key);

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

      ts_now = (*alm_ps->get_stamp)();    /* Calculate time due */
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
         (*alm_ps->setup)(ts_due);

         PRF(3, ("alm_start: next alarm (%p) set up (in %u us).\n",
		     alarm_list, ts_due));
      }

      do {				/* The timer interrupted */
         lock_key = intLock();
         alm_status_ps->in_use  = TRUE;
         alm_status_ps->woke_up = FALSE;
         intUnlock(lock_key);

         DBG(3, "alm_start: checking for alarms due.");
         alm_check();		/* Check for and post alarms due */

         lock_key = intLock();
         alm_status_ps->in_use  = FALSE;
         intUnlock(lock_key);
      } while (alm_status_ps->woke_up);

      UNLOCK(alm_lock);

      DBG(5, "Leaving alm_start.");
      DBG(3, "==============================================================");

      return(new_p);		/* Return alarm id */
   }
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
   register Alarm_Entry* act_p=NULL;
   unsigned long         ts_diff=0;
   register int          lock_key=0;


   DBG(5, "Entering alm_cancel.");
      
   if ( !alm_ps || !alm_status_ps ) {
     logMsg("alm_cancel: low-level alarm symbol table has not been initizlized. Call almInit().\n",
             0,0,0,0,0,0);
     DBG(5, "Leaving alm_cancel");
     return;
  }
   
   LOCK(alm_lock);
  
   lock_key = intLock();
   alm_status_ps->in_use = TRUE;
   intUnlock(lock_key);

   act_p  = alarm_list;

   if (!act_p) {
      DBG(3, "alm_cancel: Empty alarm list.");
      lock_key = intLock();
      alm_status_ps->in_use  = FALSE;
      intUnlock(lock_key);
      UNLOCK(alm_lock);
      DBG(5, "Leaving alm_cancel.");
      return;		/* Return on empty alarm list */
   }
   
   if (act_p == id) {		/* Cancel the running alarm */
      (*alm_ps->disable)();
      PRF(3, ("alm_cancel: current alarm (%p) stopped.\n", id));

      alm_discard(id);		/* Discard it */

      if (alarm_list != NULL) {	/* If there's another alarm left, */
				/* set up counter for new alarm */

	 ASSERT_STRUC(alarm_list, Alarm_Entry);

	 ts_diff = alarm_list->time_due - (*alm_ps->get_stamp)();

	 (*alm_ps->setup)(ts_diff);
	 PRF(3, ("alm_cancel: next alarm (%p) set up (in %d us).\n",
		     alarm_list, ts_diff));
      }

   } else {			/* Not the running alarm */

      alm_discard(id);		/* Discard the alarm */

   }

   do {				/* The timer interrupted */
      lock_key = intLock();
      alm_status_ps->in_use  = TRUE;
      alm_status_ps->woke_up = FALSE;
      intUnlock(lock_key);

      DBG(3, "alm_cancel: int handler was called.");
      alm_check();		/* Check for and post alarms due */

      lock_key = intLock();
      alm_status_ps->in_use  = FALSE;
      intUnlock(lock_key);
   } while (alm_status_ps->woke_up);

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
   register Alarm_Entry* act_p=NULL;
   register int          lock_key=0;
   unsigned short        i = 0;
   unsigned long         len;


   if ( !alm_ps || !alm_status_ps ) {
      printf("Alarm Info\n");
      printf("----------\n");
      printf("NOT AVAILABLE: almInit() has not been called.\n");    
      return;
   }
   

   printf("Alarm Info\n"
	  "----------\n"
	  "Last check was at %lu; time now is %lu; alarm ",
	  last_checked,
	  (*alm_ps->get_stamp)()
      );

   if (alm_status_ps->running)
      printf("is ");
   else
      printf("is NOT ");
      
   printf("RUNNING;\npending Alarms are:\n"
      );
   
   LOCK(alm_lock);
   lock_key = intLock();
   alm_status_ps->in_use = TRUE;
   intUnlock(lock_key);

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
	 
	 printf("%10lu %p %p ",
		act_p->time_due,
		act_p,
		act_p->sem_p
	    );

	 if (act_p->cnt_p)
	    printf("%8lx\n", *(act_p->cnt_p));
	 else
	    printf(" ------\n");

	 act_p = act_p->next_p;
      } while (act_p != alarm_list);
   
      len=alm_status_ps->entries * sizeof(Alarm_Entry);
   
      printf("%d used out of %d allocated alarm entries (%ld bytes).\n",
	     i,alm_status_ps->entries,len);
      

   do {				/* The timer interrupted */
      lock_key = intLock();
      alm_status_ps->in_use  = TRUE;
      alm_status_ps->woke_up = FALSE;
      intUnlock(lock_key);

      DBG(3, "almInfo: checking for alarms.");
      alm_check();		/* Check for and post alarms due */

      lock_key = intLock();
      alm_status_ps->in_use  = FALSE;
      intUnlock(lock_key);
   } while (alm_status_ps->woke_up);

   UNLOCK(alm_lock);   
}


/*+**************************************************************************
 *
 * Function:	alm_status_show
 *
 * Description:	Prints debug info about the alm_status global structure.
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

void alm_status_show (unsigned char verb)
{
    alm_status_tu   status_u;
    int             lock_key=0;
    unsigned short  len=0;
    unsigned long   level=0;
    unsigned long   vector=0;
 

    if ( !alm_ps || !alm_status_ps ) {
       printf("Alarm Status Info\n"
	      "------------------\n"
	      "NOT AVAILABLE:  almInit() has not been called.\n");
       return;
    }
    
    
    lock_key = intLock();
    memmove((void *)&status_u._i,(void *)alm_status_ps,sizeof(status_u._i));
    intUnlock(lock_key);

    printf("Alarm Status Info\n"
	   "------------------\n"
	   "Last check was at %lu; time now is %lu; status is 0x%lx\n",
	   last_checked,
	   (*alm_ps->get_stamp)(),
           status_u._i);

    if ( verb ) { 
         len = status_u._s.entries;
         printf("Bit 0:     Initialization %s done.\n",
               (status_u._s.init_d)   ? "IS"   : "IS NOT");
         printf("Bit 1:     Alarm %s running.\n",
               (status_u._s.running) ? "IS"    : "IS NOT");
         printf("Bit 2:     Alarm list %s in use.\n",
               (status_u._s.in_use)   ? "IS"   : "IS NOT");
         printf("Bit 3:     Interrupt handler %s awake.\n",
               (status_u._s.woke_up)  ? "IS"   : "IS NOT");
         printf("Bit 4-11:  Num of used alarm entries is %hd.\n",len);

         level  = status_u._s.int_pri;
         vector = status_u._s.int_vec;
         printf("Bit 12-15: Interrupt priority level is %ld.\n",level);
         printf("Bit 16-23: Interrupt vector offset is %ld.\n",vector);

         printf("Bit 24:    Initialization has %s started.\n",
               (status_u._s.init_s)   ? "BEEN" : "NOT BEEN");
         printf("Bit 25:    Timer %s connected to interrupt vector %ld.\n",
                (status_u._s.conn_t)   ? "IS"   : "IS NOT",vector);

         printf("Bit 26-32: Not used.\n");
    }
    return;
}


/*+**************************************************************************
 *
 * Function:	alm_int_handler
 *
 * Description:	Interrupt handler (gets called at counter overflow).
 *              The counter is disabled and a user routine is called
 *              (if it was connected by alm_start).
 *
 * Arg(s) In:	arg  -  Standard interrupt handler argument.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None.
 *
 **************************************************************************-*/

void alm_int_handler(int arg)
{
   if (almLibIsPrimitive) {
      assert(alm_ps);
      /* assert(wake_up_sem); */
      if (!wake_up_sem) {
         logMsg("almLib(interrupt handler) warning: wake_up_sem is 0\n",0,0,0,0,0,0);
      } else {
         sem_post(wake_up_sem);
      }
      alm_ps->int_ack();
   } else {
      if ( !alm_ps ) {
        DBG(3,"alm_int_handler: alarm low-level timer symbol table has not been initialized. Call almInit().");
        return;
      }

      (*alm_ps->int_ack)();      /* acknowledge the interrupt */

      if (alm_status_ps && alm_status_ps->in_use) {   /* User call is active    */
          alm_status_ps->woke_up = TRUE;
          DBG(3, "alm_int_handler: Handler woke up.");
      } else  { 
          /* Since alm_check() doesn't access alm_status_ps then
           * call it. Note that even if later alm_check() does access
           * alm_status_ps it should also be modified to add a check for
           * the existence of this pointer 
           */
                               /* No users around           */
          alm_check();         /* Check for and post alarms */
      }
   }
}


/*+**************************************************************************
 * 
 * Function:    alm_intLevelSet
 *
 * Description: This routine sets the interrupt vector priority level 
 *              in a local variable which will be used when alm_setup()
 *              or alm_enable are called. Please be aware that
 *              an expert users should be consulted when changing
 *              the timer interrupt priority level.
 *
 *              The valid interrupt level range is 0-7. However, an 
 *              interrupt level of 0 disables interrupts.
 * 
 * Arg(s) In:   level - Interrupt priority level (0-7)
 *                      Note: an interrupt level of 0 will disable interrupts.
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Error code:
 *                    OK    - Successfully set interrupt level locally
 *                    ERROR - Failed to set interrupt level locally
 *
 **************************************************************************-*/ 
long alm_intLevelSet(int level)
{
    long status=OK;
    
    if ( (level<0) || (level>7) ) {
       logMsg("alm_intLevelSet: Invalid interrupt level of %d val (0-7 inclusive)\n",level,0,0,0,0,0);
       status=ERROR;    
    }    
    else if ( !alm_status_ps ) {
       logMsg("alm_intLevelSet: low-level alarm symbol table has not been initizlized. Call almInit().\n",
               0,0,0,0,0,0);
       status = ERROR;
    }
    else {
       if ( level==0 ) 
          logMsg("alm_intLevelSet: Warning interrupt level of zero disables interrupts.\n",0,0,0,0,0,0);
       alm_status_ps->int_pri = level;
    }
    return(status);  
}

/*+**************************************************************************
 *              Wrapper Functions for low-level timer routines
 *                         (BSP Specific)
 **************************************************************************-*/
 

/*+**************************************************************************
 *
 * Function:	alm_get_timestamp  (Wrapper function)
 *
 * Description:	This is a wrapper calls the low-level timer
 *              function that returns the 32-bit timestamp value
 *              with a 1us resolution.
 *
 * Arg(s) In:	None
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Time stamp value.(in microseconds)
 *
 **************************************************************************-*/
 
unsigned long alm_get_stamp(void)
{
   unsigned long    val=0;

   if ( !alm_ps ) 
     logMsg("alm_get_stamp: low-level alarm symbol table has not been initizlized. Call almInit().\n",
            0,0,0,0,0,0);
   else
     val = (*alm_ps->get_stamp)();      /* get the timestamp  */
   return(val); 
}



/*+**************************************************************************
 *
 * Function:	alm_setup  (Wrapper function)
 *
 * Description:	This is a wrapper calls the low-level timer
 *              function to setup the timer counter register and 
 *              initiate counting.
 *
 * Arg(s) In:	delay  -  Demanded timer delay in microseconds
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	None
 *
 **************************************************************************-*/
 
void alm_setup(unsigned long delay)
{    
    if ( !alm_ps ) 
     logMsg("alm_setup: low-level alarm symbol table has not been initizlized. Call almInit().\n",
            0,0,0,0,0,0);
    else
      (*alm_ps->setup)(delay);   /* setup the time delay and begin counting */
   return;
}


/*+**************************************************************************
 *
 * Function:	alm_reset  (Wrapper function)
 *
 * Description:	This is a wrapper calls the low-level timer function
 *              that resets the timer comparator counter to 0 for the mv162.
 *              Howecver, for the powerPc it sets the decrementer counter
 *              to the maximum value and disables counting.
 *
 * Arg(s) In:	enb    - flag to enable (1) or inhibit counting 
 *                       not used.
 *
 * Arg(s) Out:	None.
 *
 * Return(s):	Status code:
 *                   OK    - Successful completion 
 *                   ERROR - Operation failed
 *
 **************************************************************************-*/
 
long alm_reset(int enb)

{
    long   status=ERROR;
    
    if ( !alm_ps ) 
       logMsg("alm_reset: low-level alarm symbol table has not been initizlized. Call almInit().\n",
            0,0,0,0,0,0);
    else
       status = (*alm_ps->reset)(enb);   /* Reset the counter, or disable it */
    return(status);
}


/*+**************************************************************************
 * 
 * Function:    alm_intVecGet
 *  
 * Description: This routine returns the interupt vector table offset
 *              used for timer #3. However, this does not indicate if
 *              the alarm interrupt handler has alreay been setup at
 *              this address.
 *
 * Arg(s) In:   None
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector table offset
 *
 **************************************************************************-*/
 
unsigned long alm_intVecGet(void)
{
   unsigned long vector = 0;
   if ( alm_ps ) 
     vector = (*alm_ps->intVecGet)();
   return(vector);
}
/*+**************************************************************************
 * 
 * Function:    alm_intLevelGet
 *
 * Description:  This routine returns interrupt vector priority level.
 *               currently set.
 *
 * Arg(s) In:   None
 * 
 * Arg(s) Out:  None.
 *
 * Return(s):   Interrupt vector priority level (0-7).
 *
 **************************************************************************-*/
 
int alm_intLevelGet(void)
{
    int level = 0;
    
    if (alm_ps) 
      level = (*alm_ps->intLevelGet)();   
   return(level);
}

/*+**************************************************************************
 *
 * Project:	Experimental Physics and Industrial Control System (EPICS)
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	almLib.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		Contains the hardware inpependent parts of this library.
 *		The appropriate lower level driver code is included.
 *		the target architecture should be cpp-defined:
 *		mv162    -  for Motorola's MVME162 board family
 *		vmod60   -  Janz VMOD-60 cpu
 *		eltec27  -  Eltec Eurocom E27
 *
 * To Do:
 *		- Should auto-configure at almInit() instead of setting
 *		  fixed values after querying the cpu speed.
 *              - Should use VME2chip to work on mv167
 *
 * Author(s):	Ralph Lange
 *
 * $Log: almLib.c,v $
 * Revision 2.9.2.2  2006/06/09 09:54:09  franksen
 * for unknown reasons, we get an interrupt before alm_start has been called;
 * therefore replaced assertion by a test and a warning message
 *
 * Revision 2.9.2.1  2006/06/08 12:43:58  franksen
 * alternatively use an extremely simple implementation that can only be used by one task
 *
 * Revision 2.9  2004/07/13 13:14:24  luchini
 * Add wrapper functions for low-level timer support
 *
 * Revision 2.8  2004/06/24 11:22:46  luchini
 * rename almStatus to alm_status_show
 *
 * Revision 2.7  2004/06/22 12:04:22  luchini
 * Add functions & chg var status to global var alm_status
 *
 * Revision 2.6  2004/04/02 14:18:43  luchini
 * fixed typo, intLock to intUnlock in alm_start()
 *
 * Revision 2.5  2004/03/29 13:29:14  lange
 * Bugfix: Guard bitfield operations
 *
 * Revision 2.4  1999/09/08 18:03:10  lange
 * Fixed more Tornado101 warnings
 *
 * Revision 2.3  1999/09/08 17:40:22  lange
 * Fixed Tornado101 warnings
 *
 * Revision 2.2  1997/07/31 18:07:03  lange
 * alm -> almLib
 *
 * Revision 2.1  1997/07/31 17:50:23  lange
 * alm_init -> almInit, reflects mv162 cpu speed.
 *
 * Revision 2.0  1997/02/07 16:30:46  lange
 * Changed interface; alm is standalone module now.
 *
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
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/
