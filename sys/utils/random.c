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

/* random.c -- random-number generator implementations for Bear
*/

#include <constants.h>
#include <kmalloc.h>
#include <random.h>
#include <stdint.h>
#include <kstring.h>

#define INTEL_HW_RNG

#if defined(INTEL_HW_RNG)

#define RDRAND_LONG ".byte 0x48,0x0f,0xc7,0xf0"

/* Intel hardware from Ivy Bridge onward provides a RNG for us. */
void srandom(uint32_t _) { }
uint64_t random() {
	uint64_t rand;
	uint64_t *randptr = &rand;

	asm volatile("0: " RDRAND_LONG "\n\t"
		     : "=a"(*randptr) );

	return rand;
}
#elif defined(DECENT_RNG)
#else
/* Basic, old, and probably insecure RNG. */
#define NSHUFF 50       /* to drop some "seed -> 1st value" linearity */
static uint32_t randseed = 937186357;

void srandom(uint32_t seed) {
	int i;

	randseed = seed;
	for (i = 0; i < NSHUFF; i++)
		(void)random();
}

/*
 * Pseudo-random number generator for randomizing the profiling clock,
 * and whatever else we might use it for.  The result is uniform on
 * [0, 2^31 - 1].
 */
uint32_t random() {
	register uint32_t x, hi, lo, t;

	/*
	 * Compute x[n + 1] = (7^5 * x[n]) mod (2^31 - 1).
	 * From "Random number generators: good ones are hard to find",
	 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
	 * October 1988, p. 1195.
	 */
	/* Can't be initialized with 0, so use another value. */
	if ((x = randseed) == 0)
		x = 123459876;
	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t < 0)
		t += 0x7fffffff;
	randseed = t;

	return (t);
}
#endif

/* Returns a random number between [low, high) */
uint64_t random_between(uint64_t low, uint64_t high) {
	return (random() % (high - low)) + low;
}

/* Returns a random number between [low, high) */
uint64_t random_between_align(uint64_t low, uint64_t high, uint64_t align) {
  uint64_t ret;

  do {
    ret = random_between(low, high);
  } while ( (ret & ~(align - 1)) < low );

  return ret & ~(align - 1);
}

/* Shuffle an arbitrary array, giving it a random order. We use the Fisher-Yates
 * shuffle algorithm (also known as the Knuth shuffle). */
void random_shuffle(void *array, size_t elements, size_t size) {
	uint8_t *s;
	uint8_t *d;
	uint8_t *tmp;
	int i;
	int j;

	tmp = kmalloc_track(RANDOM_SITE,size);
	for(i = elements - 1; i > 0; i--) {
		j = random_between(0, i+1);
		if(i == j)
			continue;
		s = (uint8_t *)array + i*size;
		d = (uint8_t *)array + j*size;
		kmemcpy(tmp, s, size);
		kmemcpy(s, d, size);
		kmemcpy(d, tmp, size);
	}
	kfree_track(RANDOM_SITE,tmp);
}
