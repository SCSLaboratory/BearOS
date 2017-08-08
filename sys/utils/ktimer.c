/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

/*******************************************************************************
* Filename: ktimer.c
*
* Description: 
* Watchdog timers for the kernel. Inspired by Minix's implementation.
*
******************************************************************************/

#include <constants.h>  /* For public/static  */
#include <kernel.h>     /* For kpanic          */
#include <stdint.h>     /* For uint types      */
#include <kmalloc.h>    /* For kmalloc         */
#include <kqueue.h>      /* For queue           */
#include <interrupts.h> /* For PIT timing info */
#include <ktimer.h>
#include <tsc.h>

/*****************************************************************************
 **************************** PRIVATE DECLARATIONS ***************************
 ****************************************************************************/

/* Represents a single alarm */
typedef struct {
	int64_t ms;
	void (*cb_func)(void*);
	void *arg;
} Alarm_t;

/* Decrement the time left on an alarm */
static void decrement_alarm(void*, void*);

/* Returns nonzero if an alarm's timer has expired */
static int is_timer_expired(void*, const void*);

/* Finds the number of ms until the next alarm will go off */
static void find_wait_time(void*, void*);

/* Decrement all alarms by the correct amt and run the ones that have expired */
static void service_alarms();


/*** STATIC VARIALBES ***/

/* Queue of pending alarms */
static void     *alarms;

/* time left on shortest alarm. 0 means no pending alarms */
static int64_t  ms_remaining = 0;

/* ms between time of last alarm and time of next alarm. */
static int64_t  alarm_len = 0;

/* ms since start of hardware timer (+/- MS_PER_TICK) */
static uint64_t total_ms_elapsed = 0;

static uint64_t tsc_freq, tsc_last, tsc_cur =0;

/******************************************************************************
 ****************************** PUBLIC FUNCTIONS ******************************
 *****************************************************************************/

/* Initialization function for this module. */
int ktimer_init() {

	alarms = qopen();

	tsc_freq =	get_tsc_freq();
	tsc_last = readtscp();	
	
	return 0;
}

/* Must be called every MS_PER_TICK milliseconds. */
void ktimer_tick() {

	tsc_cur = readtscp();
	
	if((tsc_cur - tsc_last ) >  (tsc_freq /1000))
	{
		
		/* Update total ms elapsed */
		/*we're now firing in microseconds !! */
		total_ms_elapsed += 1;

		/* If alarms are pending, decrement time remaining */
		if (ms_remaining != 0) { 
		
			ms_remaining -= 1;              /* Decrement shortest alarm */

			if (ms_remaining <= 0)     /* At least one alarm should be triggered. */
				service_alarms();
		}
		tsc_last = tsc_cur;
	}
	
}

uint64_t ktimer_get_ms_elapsed() {
	return total_ms_elapsed;
}

/*
 * Function: 
 *  ktimer_new_alarm()
 *
 * Description:
 *  Register a new one-shot alarm to be triggered at a later point in time.
 * 
 * Arguments:
 *  uint32_t ms - # milliseconds to wait before calling callback_func
 *  void (*callback_func)(void*) - callback function to be called after ms millisecs
 *  void *arg - argument to be passed to the callback function.
 *
 */
void ktimer_new_alarm(uint32_t ms, void (*callback_func)(void*), void *arg) {
	Alarm_t *new_alarm;

  /*hack if you make the timer in microseconds 
   currently ours is set to 100us */ 
 
 	//kprintf("adding new alarm %x \n", callback_func);

	/* Create new alarm */
	new_alarm = kmalloc_track(KTIMER_SITE,sizeof(Alarm_t));
#ifdef KERNEL
	if (new_alarm == NULL)
		kpanic("ERROR: Could not allocate alarm\n");
#endif
	new_alarm->ms      = (int64_t)ms;
	new_alarm->cb_func = callback_func;
	new_alarm->arg     = arg;

	/* Put into queue */
	qput(alarms,new_alarm);

	/* See if this alarm will be the next one to occur */
	if (((int64_t)(ms) < ms_remaining) || (alarm_len == 0)) {
		alarm_len = (int64_t)(ms) + (alarm_len - ms_remaining);
		ms_remaining = (int64_t)ms;
	}
}


/****************************** PRIVATE FUNCTIONS *****************************/

/* Decrement all alarms by the correct amt and run the ones that have expired */
static void service_alarms() {
	int64_t delta;
	Alarm_t *a;

	/* Decrement all alarm counters. */
	delta = alarm_len - ms_remaining;                /* ms since last update */
	qapply2(alarms, (void*)(&delta),decrement_alarm);    /* Perform update   */

	/* Service and free any expired alarms. */
	a = qremove(alarms,is_timer_expired,NULL);
	while (a != NULL) {

		if ((a->cb_func) != NULL)        /* Call callback function if exists */
			(a->cb_func)(a->arg);
		kfree_track(KTIMER_SITE,a);

		a = qremove(alarms,is_timer_expired,NULL);         /* Get next alarm */
	}

	/* Find the length of the shortest alarm in the queue (0 if empty) */
	ms_remaining = 0;
	qapply2(alarms,(void*)(&ms_remaining),find_wait_time);
	alarm_len = ms_remaining;
}

/* 
 * Called every MS_PER_TICK ms on every pending alarm; decrements the time
 * remaining for a single alarm.
 */
static void decrement_alarm(void *arg, void *elementp) {
	int64_t delta;
	Alarm_t *alarm;

	delta = *(int64_t *)(arg);
	alarm = (Alarm_t *)elementp;

	alarm->ms = alarm->ms - delta;
}

/* Returns true if the timer on an alarm has expired. */
static int is_timer_expired(void *elementp, const void *keyp) {
	Alarm_t *alarm;

	alarm = (Alarm_t *)elementp;
	return ((alarm->ms) <= 0);
}

/* 
 * When called on a queue of alarms, it will find the shortest wait time
 * remaining and set the integer pointed to by pdelta.
 */
static void find_wait_time(void *arg, void *elementp) {
	int64_t *pms_remaining;
	Alarm_t  *alarm;

	pms_remaining = (int64_t *)arg;
	alarm  = (Alarm_t  *)elementp;

	if ((*pms_remaining) == 0)
		*pms_remaining = alarm->ms;
	else if ((alarm->ms) < (*pms_remaining))
		*pms_remaining = alarm->ms;
}
