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

#pragma once
#include <stdint.h>

/*
 *
 * Public interface to semaphores
 *
 */
typedef void* sem_queue_t;

typedef struct queue_element_t
{
  int PID;
} queue_element_t;

typedef struct semaphore_t
{
  uint64_t s_counter;
  int owner;
  sem_queue_t q;
} semaphore_t;

#define NO_OWNER -1

#define MUTEX ((struct semaphore_t) {1, 0})

/* Signal/increase semaphore counter */
void s_signal( semaphore_t *sem );

/* Wait/decrease semaphore counter */
void s_wait( semaphore_t *sem, int PID );

/*a spinlock to acquire the semaphore*/
void acquire_lock(volatile semaphore_t *sem);

/*releases the semaphore*/
void release_lock(volatile semaphore_t *sem);

/* Create a new semaphore with counter c */
semaphore_t* create_semaphore( int counter );

/* Deallocate a semaphore */
void delete_semaphore( semaphore_t *sem );
