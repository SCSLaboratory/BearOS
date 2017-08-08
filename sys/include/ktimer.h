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
/******************************************************************************
* Filename: ktimer.h
*
* Description: 
* Watchdog timers for the kernel. Inspired by Minix's implementation.
*
******************************************************************************/

#include <constants.h>  /* For public/private */
#include <stdint.h>

/* Called on startup to initialize queue, etc. */
int ktimer_init();

/* Called every time the PIT timer goes off */
void ktimer_tick();

/* Returns the number of ms that have elapsed since the timer started counting. */
uint64_t ktimer_get_ms_elapsed();

/* 
 * Creates a new alarm that will call callback_func() with the
 * argument 'arg'. 
 */
void ktimer_new_alarm(uint32_t ms, void (*callback_func)(void*), void *arg);


