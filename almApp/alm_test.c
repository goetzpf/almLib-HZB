/* $RCSfile: alm_test.c,v $ */

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_test.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		Test Routines.
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 2.3 $
 * $Date: 1999/09/08 18:03:10 $
 *
 * $Author: lange $
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
rcsid[] = "@(#)almLib: $Id: alm_test.c,v 2.3 1999/09/08 18:03:10 lange Exp $";


#include <semaphore.h>
#include <stdio.h>
#include <taskLib.h>

extern void logMsg (char*, ...);

#include <almLib.h>

static sem_t lock, info_lock;
static char init_d = FALSE;
static int finish;

typedef struct
{
   unsigned long delay;
   long max;
   long min;
   double avg;
   unsigned short cnt;
} Info_Entry;

static Info_Entry info[10] =
{
 {       10ul, LONG_MIN,LONG_MAX,0,0 } ,
 {      100ul, LONG_MIN,LONG_MAX,0,0 } ,
 {      200ul, LONG_MIN,LONG_MAX,0,0 } ,
 {      500ul, LONG_MIN,LONG_MAX,0,0 } ,
 {     1000ul, LONG_MIN,LONG_MAX,0,0 } ,

 {     2000ul, LONG_MIN,LONG_MAX,0,0 } ,
 {     5000ul, LONG_MIN,LONG_MAX,0,0 } ,
 {    10000ul, LONG_MIN,LONG_MAX,0,0 } ,
 {   100000ul, LONG_MIN,LONG_MAX,0,0 } ,
 {  1000000ul, LONG_MIN,LONG_MAX,0,0 }
};


int ala_t (int i, int cnt);
int stamp_t (int i, int cnt);

int
atest (int no_of_tasks, int no_of_runs)
{
   int i;
   
   if (!init_d) {
      if (sem_init(&lock, 0, 1)) {
	 return(-1);
      }
      if (sem_init(&info_lock, 0, 1)) {
	 return(-1);
      }
      init_d = TRUE;
   }

   finish = no_of_tasks;

   for (i = 0; i < no_of_tasks; i++) {

      taskSpawn("a-tst", 40, 0, 3000,
		(FUNCPTR) ala_t, i, no_of_runs, 0,0,0,0,0,0,0,0);

   }

   return(0);
}

int
stest (int no_of_tasks, int no_of_runs)
{
   int i;
   
   if (!init_d) {
      if (sem_init(&lock, 0, 1)) {
	 return(-1);
      }
      if (sem_init(&info_lock, 0, 1)) {
	 return(-1);
      }
      init_d = TRUE;
   }

   finish = no_of_tasks;

   for (i = 0; i < no_of_tasks; i++) {

      taskSpawn("s-tst", 40, 0, 3000,
		(FUNCPTR) stamp_t, i, no_of_runs, 0,0,0,0,0,0,0,0);

   }

   return(0);
}

void
a_finito()
{
   sem_wait(&lock);
   finish--;
   if (finish <= 0)
      logMsg("--------------- Finished. ------------------\n");
   sem_post(&lock);
}

int
ala_t (int my_no, int cnt)
{
   sem_t sema;
   unsigned short i,j;
   Info_Entry* inf_p;
   unsigned long start, stop, delta;
   long diff;
   
   if (sem_init(&sema, 0, 0)) {
      logMsg("Sem_init failed.\n");
      return(-1);
   }

   sem_wait(&lock);
   logMsg ("Task %d started.\n", my_no);
   sem_post(&lock);

   i = my_no % 10;
   for (j = 0; j < (cnt * 10); j++) {
      
      inf_p = &info[i];
      
#if 0
      sem_wait(&lock);
      logMsg("Task %d: waiting %u us ...\n", my_no, inf_p->delay);
      sem_post(&lock);
#endif

      start = alm_get_stamp();

      (void) alm_start(inf_p->delay, &sema, NULL);
      sem_wait(&sema);

      stop  = alm_get_stamp();
      delta = stop - start;
      diff  = delta - inf_p->delay;

      if (diff < 0)
	 logMsg("Delay: %lu Delta: %lu Diff: %ld.\n",
		inf_p->delay, delta, diff);

      sem_wait(&info_lock);

      inf_p->cnt++;
      if (diff < inf_p->min) inf_p->min = diff;
      if (diff > inf_p->max) inf_p->max = diff;
      inf_p->avg = (inf_p->avg * ((double) (inf_p->cnt - 1) / inf_p->cnt))
	 + ((double) diff / inf_p->cnt);

      sem_post(&info_lock);

      i = (i + 1) % 10;

/*      taskDelay(my_no); */
      
   }
   a_finito();
   return(0);
}


int
stamp_t (int my_no, int cnt)
{
   sem_t sema;
   unsigned short j;
   unsigned long t1, t2;
   long diff;
   
   if (sem_init(&sema, 0, 0)) {
      logMsg("Sem_init failed.\n");
      return(-1);
   }

   sem_wait(&lock);
   logMsg ("Task %d started.\n", my_no);
   sem_post(&lock);

   for (j = 0; j < cnt; j++) {
      
      t1 = alm_get_stamp();
      
      taskDelay(3);

      t2 = alm_get_stamp();
      diff = t2 - t1;

      if (diff < 0)
	 logMsg("ERROR - T1: %lu T2: %lu Diff: %ld.\n",
		t1, t2, diff);

   }
   a_finito();
   return(0);
}

void
aInfo (void)
{
   int i;
   
   printf("Alarm Test Stats\n"
	  "----------------\n"
	  "Delay    No         Min       Avg         Max\n"
	  "-----------------------------------------------\n"
      );
   
   sem_wait(&info_lock);

   for (i = 0; i < 10; i++) {
      printf("%7lu %5u %11ld %9.2f %11ld\n",
	     info[i].delay,
	     info[i].cnt,
	     info[i].min,
	     info[i].avg,
	     info[i].max
	 );
   }
   
   sem_post(&info_lock);
}

void
aClear (void)
{
   int i;
   
   for (i = 0; i < 10; i++) {
      
      info[i].cnt = 0;
      info[i].min = LONG_MAX;
      info[i].avg = 0.0;
      info[i].max = LONG_MIN;
   }
}

void
aSet (unsigned long delay)
{
   sem_t sema;

   if (sem_init(&sema, 0, 0)) {
      logMsg("Sem_init failed.\n");
      return;
   }

   (void) alm_start(delay, &sema, NULL);
   sem_wait(&sema);

   return;
}

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	Alm - High Resolution Timer and Alarm Clock Library
 *
 * File:	alm_test.c
 *
 * Description:	Library package to implement a high resolution alarm clock.
 *		Test Routines.
 *
 * Author(s):	Ralph Lange
 *
 * $Log: alm_test.c,v $
 * Revision 2.3  1999/09/08 18:03:10  lange
 * Fixed more Tornado101 warnings
 *
 * Revision 2.2  1999/09/08 17:40:22  lange
 * Fixed Tornado101 warnings
 *
 * Revision 2.1  1997/07/31 18:07:09  lange
 * alm -> almLib
 *
 * Revision 2.0  1997/02/07 16:30:50  lange
 * Changed interface; alm is standalone module now.
 *
 * Revision 1.1  1997/02/07 16:04:46  lange
 * Added counter increment; made alm a module of its own.
 *
 * Copyright (c) 1996, 1997, 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/
