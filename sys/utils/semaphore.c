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

/*
 * Implements a semaphore
 *
 */
#include <apic.h> 
#include <semaphore.h>
#include <kqueue.h>
#include <kmalloc.h>
#include <constants.h>


/* need to use xchg operation when spinning on locks as xchg is atomic
   and will avoid possible race conditions*/
/* a value is returned for the aquire lock function, which is a spinlock*/
static uint64_t exchange(volatile uint64_t *sem_addr, uint64_t value)
{
  uint64_t ret_value;
  
  asm volatile("lock; xchgq %0, %1" :
               "+m" (*sem_addr), "=a" (ret_value) :
               "1" (value) :
               "cc");
  return ret_value;
}

/*lock the semaphore*/
void acquire_lock(volatile semaphore_t *sem) {

  while(exchange(&sem->s_counter, 1) != 0);

  sem->owner = this_cpu();

  return;
}

/*unlock the semaphore*/
void release_lock(volatile semaphore_t *sem) {

  exchange(&sem->s_counter, 0);

  sem->owner = NO_OWNER;
 
  return;
}

void s_signal( semaphore_t *sem )
{	
  /* Increase the s_counter of the given semaphore */
  __asm__ __volatile__ ( "incq %0" :"=D"(sem->s_counter):"0"(sem->s_counter) );
	
  /* Check if there are processes in the queue that could be scheduld now */
  if( sem->s_counter > 0 )
    {
      queue_element_t *q_el = (queue_element_t*)kmalloc_track(SEMAPHORE_SITE, sizeof(queue_element_t) );
      q_el = qget( sem->q );
      if( q_el != NULL )
	{
	  s_wait( sem, q_el->PID );
	}
    }
}

void s_wait( semaphore_t *sem, int PID )
{
  /* test if the semaphore allows more ressource sharing */
  if( sem->s_counter <= 0 )
    {
      /* Add the process to the queue */	
      queue_element_t *q_el = (queue_element_t*)kmalloc_track(SEMAPHORE_SITE, sizeof(queue_element_t) );
      q_el->PID = PID;;
      qput( sem->q, q_el );
    }
  else
    {
      /* Decrease the s_counter of the given semaphore */
      __asm__ __volatile__ ( "decq %0" :"=D"(sem->s_counter):"0"(sem->s_counter) );
    }
}

semaphore_t* create_semaphore( int counter )
{
  semaphore_t *sem;
  sem = (semaphore_t*)kmalloc_track(SEMAPHORE_SITE, sizeof(semaphore_t) );
	
  sem->s_counter = counter;
  sem->owner = NO_OWNER;
	
  sem_queue_t q;
  q = qopen();
  sem->q = q;  
	
  return sem;
}

void delete_semaphore( semaphore_t *sem )
{
  /* check if the semaphores queue is empty */
  sem_queue_t *q = sem->q;

  if( qget( q ) == NULL )
    {
      qclose( q );
      kfree_track(SEMAPHORE_SITE, sem ); /* the queue is empty */
    }
  else
    {
      kfree_track(SEMAPHORE_SITE, sem );
      /* Check if the counter permits the remaining processes to run */
		
      /* CAN THAT HAPPEN? TODO */
    }
}
