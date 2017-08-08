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
 * Filename: random.h
 *
 * Description:
 *  API for generating pseudorandom numbers in the kernel
 *
 *****************************************************************************/

#include <constants.h>
#include <stdint.h>

/* Seed the random number generator.  Should be called before random() */
void srandom(uint32_t seed);

/* Returns a random number. */
uint64_t random();

/* Returns a random unsigned long betwen [low, high). */
uint64_t random_between(uint64_t low, uint64_t high);

/* Returns a random unsigned long betwen [low, high). */
uint64_t random_between_align(uint64_t low, uint64_t high, uint64_t align);

/* Randomly shuffle an arbitrary array. */
void random_shuffle(void *array, size_t elements, size_t size);
