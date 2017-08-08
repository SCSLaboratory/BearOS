
#pragma once
#include <stdint.h>

/*
 *
 * Public interface to simple spin lock
 *
 */

typedef uint64_t lock_t;

/* a spinlock to aquire the lock */
void aquire_lock(volatile lock_t *lock);

/*releases the semaphore*/
void release_lock(volatile lock_t *lock);

/* Create a new semaphore with counter c */
void init_lock( lock_t * );
