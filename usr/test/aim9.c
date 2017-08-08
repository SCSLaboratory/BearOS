/*
  modified versions of open souce aim9 test suite and the Lever and Boreham 
  (2000) malloc test.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <newlib.h>
#include <syscall.h>
#include <msg.h>
#include <errno.h>
#include <aim9.h>

/* fixes discrepancy between kernel and user */
#define CLOCK_MONOTONIC 1	

/* Malloc test value */
#define MAX_MALLOC_PROCESSES 100
static unsigned size = 1024;
static unsigned iteration_count = 10000000;
/* end malloc test values */

#define MAX_AIM9_ITERATIONS 100

/* FUNCTIONS
 *
 */


void main(int argc, char *argv[]) {
  int ret;
  ret = aim9_tests();
  exit(ret);
}

/*

  BENCHMARK HAS THE FOLLOWING FUNCTIONS SO FAR

  void fork_test()
  void exec_test()
*/

static inline uint64_t readtsc() {
  uint32_t lo, hi;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx" );
  return (uint64_t)(lo) | ((uint64_t)(hi) << 32);
}


/* http://www.citi.umich.edu/projects/linux-scalability/reports/malloc.html */
static void *dummy(unsigned i){
  return NULL;
}

static void malloc_test(){
  unsigned int i;
  unsigned request_size = size;
  unsigned total_iterations = iteration_count;
  struct timespec start, end, null, elapsed, adjusted;
  uint64_t cnt_before_adj, cnt_after_adj, cnt_before_test, cnt_after_test;
  uint64_t total_cnt, total_cnt_adj;

  /*
   * Time a null loop.  We'll subtract this from the final
   * malloc loop results to get a more accurate value.
   */
  clock_gettime(CLOCK_MONOTONIC, &start);
  cnt_before_adj = readtsc();

  for (i = 0; i < total_iterations; i++) {
    void * buf;
    buf = dummy(i);
    buf = dummy(i);
  }

  cnt_after_adj = readtsc();

  clock_gettime(CLOCK_MONOTONIC, &end);

  null.tv_sec = end.tv_sec - start.tv_sec;
  /*  null.tv_usec = end.tv_usec - start.tv_usec;
      if (null.tv_usec < 0) {
      null.tv_sec--;
      null.tv_usec += USECSPERSEC;
      }*/

  /*
   * Run the real malloc test
   */
  clock_gettime(CLOCK_MONOTONIC, &start);
  cnt_before_test = readtsc();

  for (i = 0; i < total_iterations; i++) {
    void * buf;
    buf = malloc(request_size);
    buf = realloc(buf, request_size*2);
    free(buf);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  cnt_after_test = readtsc();

  elapsed.tv_sec = end.tv_sec - start.tv_sec;
  total_cnt = cnt_after_test-cnt_before_test;
  total_cnt_adj = total_cnt - (cnt_after_adj - cnt_before_adj);
  /*  elapsed.tv_usec = end.tv_usec - start.tv_usec;
      if (elapsed.tv_usec < 0) {
      elapsed.tv_sec--;
      /    elapsed.tv_usec += USECSPERSEC;
      }*/

  /*
   * Adjust elapsed time by null loop time
   */
  adjusted.tv_sec = elapsed.tv_sec - null.tv_sec;
  /*  adjusted.tv_usec = elapsed.tv_usec - null.tv_usec;
      if (adjusted.tv_usec < 0) {
      adjusted.tv_sec--;
      adjusted.tv_usec += USECSPERSEC;
      }*/
  /*%ld.%06ld*/
  /*  printf("total time: %d, adj time: %d, total cycles: %lu adj cycles: %lu\n"
      "iterations: %d, block size %d\n", (int)elapsed.tv_sec,
      (int)adjusted.tv_sec, total_cnt, total_cnt_adj, total_iterations,
      (int) request_size);*/

  return;
}

static void fork_test()
{
  struct timespec before, after;
  int fval, time_diff_sec;
  printf( "Fork-test\n" );

  uint64_t cnt_before, cnt_after, sum;
  int n = 1000;

  int outer_loop;
  for( outer_loop=0; outer_loop<1; outer_loop++ )
    {
      clock_gettime( CLOCK_MONOTONIC, &before );
      cnt_before = readtsc();
      for( n=0; n<1000; n++ )
	{
	  fval = fork();
	  if( fval == 0 )
	    exit(0);
	}

      cnt_after = readtsc();
      clock_gettime( CLOCK_MONOTONIC, &after );
      time_diff_sec = (after.tv_sec - before.tv_sec);
      sum += (cnt_after-cnt_before);
      n = 1000;
    }


  printf( "==> FORK <== \n" );
  printf( "Cycles: %lu\n", sum );
  printf( "Time  : %d sec\n\n", time_diff_sec );

}

static void exec_test()
{
  struct timespec before, after;
  int fval,time_diff_sec;
  printf( "Exec-test\n" );

  uint64_t cnt_before, cnt_after, sum;
  int n = 1000;

  int i;
  for( i=0; i<1; i++ )
    {
      clock_gettime( CLOCK_MONOTONIC, &before );

      cnt_before = readtsc();
      while(n--)
	{
	  fval = fork();          /* fork the task off */
	  if( fval == 0 )
	    {
	      /* we're the child */
	      exit(0);        /* quit painlessly */
	    }
	}

      cnt_after = readtsc();

      clock_gettime( CLOCK_MONOTONIC, &after );
      n = 1000;
    }

  printf( "==> EXEC <==\n" );
  printf( "Cycles: %lu\n", sum );
  printf( "Time  : %d sec.\n\n", time_diff_sec );
}

static void add_long()
{
  int n;                            /* internal loop variable */
  long l1,l2,l;

  l1 = 1294967295;                       /* use register variables */
  l2 = 2294967295;
  l = 0;
  /*
   * Variable Values 
   */
  /*
   * l    l1    l2   
   */
  for (n = 1000000; n > 0; n--) {        /*    0    x     -x  - initial value */
    l += l1;                /*    x    x     -x   */
    l1 += l2;               /*    x    0     -x   */
    l1 += l2;               /*    x    -x    -x   */
    l2 += l;                /*    x    -x    0    */
    l2 += l;                /*    x    -x    x    */
    l += l1;                /*    0    -x    x    */
    l += l1;                /*    -x   -x    x    */
    l1 += l2;               /*    -x   0     x    */
    l1 += l2;               /*    -x   x     x    */
    l2 += l;                /*    -x   x     0    */
    l2 += l;                /*    -x   x     -x   */
    l += l1;                /*    0    x     -x   */
    /*
     * Note that at loop end, l1 = -l2 
     */
    /*
     * which is as we started.  Thus, 
     */
    /*
     * the values in the loop are stable 
     */
  }
}

static void add_short()
{
  int n;                            /* internal loop variable */
  short s1,s2,s;

  s1 = 1400;
  s2 = 800;
  s = 0;
  /*
   * Variable Values 
   */
  /*
   * s    s1    s2   
   */
  for (n = 1000000; n > 0; n--) {        /*    0    x     -x  - initial value */
    s += s1;                /*    x    x     -x   */
    s1 += s2;               /*    x    0     -x   */
    s1 += s2;               /*    x    -x    -x   */
    s2 += s;                /*    x    -x    0    */
    s2 += s;                /*    x    -x    x    */
    s += s1;                /*    0    -x    x    */
    s += s1;                /*    -x   -x    x    */
    s1 += s2;               /*    -x   0     x    */
    s1 += s2;               /*    -x   x     x    */
    s2 += s;                /*    -x   x     0    */
    s2 += s;                /*    -x   x     -x   */
    s += s1;                /*    0    x     -x   */
    /*
     * Note that at loop end, s1 = -s2 
     */
    /*
     * which is as we started.  Thus, 
     */
    /*
     * the values in the loop are stable 
     */
  }
}

static void add_int()
{
  int n;                            /* internal loop variable */
  int i1, i2, i;

  i1 = 336000;                       /* use register variables */
  i2 = 122;

  i = 0;
  /*
   * Variable Values 
   */
  /*
   * i    i1    i2   
   */
  for (n = 1000000; n > 0; n--) {        /*    0    x     -x  - initial value */
    i += i1;                /*    x    x     -x   */
    i1 += i2;               /*    x    0     -x   */
    i1 += i2;               /*    x    -x    -x   */
    i2 += i;                /*    x    -x    0    */
    i2 += i;                /*    x    -x    x    */
    i += i1;                /*    0    -x    x    */
    i += i1;                /*    -x   -x    x    */
    i1 += i2;               /*    -x   0     x    */
    i1 += i2;               /*    -x   x     x    */
    i2 += i;                /*    -x   x     0    */
    i2 += i;                /*    -x   x     -x   */
    i += i1;                /*    0    x     -x   */
    /*
     * Note that at loop end, i1 = -i2 
     */
    /*
     * which is as we started.  Thus, 
     */
    /*
     * the values in the loop are stable 
     */
  }
}

static void mul_long()
{
  int n;
  long
    l1, l2,                       /* working copies */
    l;                            /* result */

  l1 = 81843629;                       /* use register variables */
  l2 = 72164391;

  l = 12345678;
  /*
   * Variable Values 
   */
  /*
   * l    l1    l2  
   */
  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    l *= l1;                /*    x    1     1   */
    l1 *= l2;               /*    x    1     1   */
    l1 *= l2;               /*    x    1     1   */
    l2 *= l1;               /*    x    1     1   */
    l2 *= l1;               /*    x    1     1   */
    l *= l2;                /*    x    1     1   */
    l *= l1;                /*    x    1     1   */
    l1 *= l2;               /*    x    1     1   */
    l1 *= l2;               /*    x    1     1   */
    l2 *= l1;               /*    x    1     1   */
    l2 *= l1;               /*    x    1     1   */
    l *= l2;                /*    x    1     1   */
    /*
     * Note that at loop end, all values 
     */
    /*
     * as we started.  Thus, the values 
     */
    /*
     * in the loop are stable 
     */
  }
}

static void mul_short()
{
  int n;
  short
    s1, s2,                       /* working copies */
    s;                            /* result */

  s1 = 2;                       /* use register variables */
  s2 = 2;

  s = 12;
  /*
   * Variable Values 
   */
  /*
   * s    s1    s2  
   */
  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    s *= s1;                /*    x    1     1   */
    s1 *= s2;               /*    x    1     1   */
    s1 *= s2;               /*    x    1     1   */
    s2 *= s1;               /*    x    1     1   */
    s2 *= s1;               /*    x    1     1   */
    s *= s2;                /*    x    1     1   */
    s *= s1;                /*    x    1     1   */
    s1 *= s2;               /*    x    1     1   */
    s1 *= s2;               /*    x    1     1   */
    s2 *= s1;               /*    x    1     1   */
    s2 *= s1;               /*    x    1     1   */
    s *= s2;                /*    x    1     1   */
    /*
     * Note that at loop end, all values 
     */
    /*
     * as we started.  Thus, the values 
     */
    /*
     * in the loop are stable 
     */
  }
}

static void mul_int()
{
  int n;
  int
    i1, i2,                       /* working copies */
    i;                            /* result */

  i1 = 67321;                       /* use register variables */
  i2 = 921307;

  i = 12345678;
  /*
   * Variable Values 
   */
  /*
   * i    i1    i2  
   */
  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    i *= i1;                /*    x    1     1   */
    i1 *= i2;               /*    x    1     1   */
    i1 *= i2;               /*    x    1     1   */
    i2 *= i1;               /*    x    1     1   */
    i2 *= i1;               /*    x    1     1   */
    i *= i2;                /*    x    1     1   */
    i *= i1;                /*    x    1     1   */
    i1 *= i2;               /*    x    1     1   */
    i1 *= i2;               /*    x    1     1   */
    i2 *= i1;               /*    x    1     1   */
    i2 *= i1;               /*    x    1     1   */
    i *= i2;                /*    x    1     1   */
    /*
     * Note that at loop end, all values 
     */
    /*
     * as we started.  Thus, the values 
     */
    /*
     * in the loop are stable 
     */
  }
}

static void div_long()
{
  int n;

  long
    i1, i2,                       /* working copies */
    i;                            /* result */

  i1 = 8153421843629;                       /* use register variables */
  i2 = 7216543224391;

  i = 12345675425628;

  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    i /= i1;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1232154345678; i1=2742143367321; i2=1226436334307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=121243141411278; i1=63124323427821; i2=12235345334307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=19896792345678; i1=273698767321; i2=12978693216307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12967899332167; i1=2897699569517321; i2=1343967956907;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1233976961238; i1=2749679673321; i2=13254352138407;

    i /= i2;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12332176543378; i1=273643234671671; i2=12233125345371;

    i /= i1;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=123432136754721; i1=3124357654311; i2=236543641423307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12421765845678; i1=273762634124; i2=122321687653321;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1826572356338; i1=25453675733567321; i2=6435546822307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=321554182123678; i1=232134823215421; i2=1224563712123831;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1421435671861238; i1=421341856257321; i2=1232156414307;

    i /= i2;                /*    x    1     1   */

  }
}

static void div_short()
{
  int n;

  short
    i1, i2,                       /* working copies */
    i;                            /* result */

  i1 = 20;                       /* use register variables */
  i2 = 2245;

  i = 1122;

  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    i /= i1;                /*    x    1     1   */
    if( i==0 || i==0 || i2==0 )
      i=1238; i1=2321; i2=14307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1278; i1=6311; i2=4307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=128; i1=2321; i2=6307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1237; i1=2821; i2=3437;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1233; i1=2741; i2=13217;

    i /= i2;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12378; i1=2771; i2=1221;

    i /= i1;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12341; i1=3121; i2=23407;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12428; i1=273; i2=121;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=123; i1=2541; i2=6437;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=32178; i1=2321; i2=1224;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1428; i1=4231; i2=1237;

    i /= i2;
  }
}

static void div_int()
{
  int n;

  int
    i1, i2,                       /* working copies */
    i;                            /* result */

  i1 = 27367321;                       /* use register variables */
  i2 = 12234307;

  i = 12345678;
        
  for (n = 1000000; n > 0; n--) {        /*    x    1     1   - initial value */
    i /= i1;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12345678; i1=27367321; i2=12234307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1212431278; i1=63127821; i2=12234307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12345678; i1=27367321; i2=123216307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12332167; i1=289517321; i2=12234307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12331238; i1=274673321; i2=132138407;

    i /= i2;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1232138; i1=27432671; i2=1231271;

    i /= i1;                /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1234321321; i1=31243511; i2=2341423307;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1242145678; i1=273634124; i2=122321321;

    i1 /= i2;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=12356338; i1=2543567321; i2=643522307;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=3213678; i1=23115421; i2=12253731;

    i2 /= i1;               /*    x    1     1   */
    if( i==0 || i1==0 || i2==0 )
      i=1421435678; i1=4213257321; i2=123214307;

    i /= i2;
  }
}

int aim9_tests(void)
{
  int i, pid, status;
  uint64_t cnt_before_add, cnt_after_add, cnt_before_global, cnt_after_global,
    cnt_before_mul, cnt_after_mul, cnt_before_div, cnt_after_div,
    cnt_malloc_after;

  struct timespec before_add, after_add ,before_global, after_global,
    before_mul, after_mul, before_div, after_div, after_malloc;

  double time;

  printf("Benchmarking MALLOC/REALLOC\n");
  clock_gettime( CLOCK_MONOTONIC, &before_global );
  cnt_before_global = readtsc();

  /*
   * Benchmark for malloc from netscape
   */
  
  for( i = 0; i <= MAX_MALLOC_PROCESSES; i++ )
    {
      pid = fork();
      if( pid == 0 ){
	malloc_test();
	exit(0);
      }
      if( i == MAX_MALLOC_PROCESSES ){
	waitpid(pid, &status, 0);
      }	

    }

  clock_gettime( CLOCK_MONOTONIC, &after_malloc );
  cnt_malloc_after = readtsc();  

  printf(" ==> MALLOC <== \n");
  printf("Time: %d sec \n",  
	 (int)(after_malloc.tv_sec - before_global.tv_sec)); 
  printf("MALLOC Cycles %lu\n",
	 cnt_malloc_after - cnt_before_global);
  //  malloc_test();
	
  /*
   * Benchmark fork syscall
   */
  //fork_test();

  /* 
   * Benchmark exec syscall
   */	
  //exec_test();
	
  /*
   * Benchmark the add operation
   */
  printf( "Benchmarking ADD\n" );
  clock_gettime( CLOCK_MONOTONIC, &before_add );
  cnt_before_add = readtsc();
  for( i=0; i<MAX_AIM9_ITERATIONS; i++ )
    {
      add_long();
      add_short();
      add_int();
    }
  cnt_after_add = readtsc();
  clock_gettime( CLOCK_MONOTONIC, &after_add );
	
  printf( " ==> ADD <== \n" );
  printf( "Time: %d sec \n", (int)(after_add.tv_sec - before_add.tv_sec) );
  printf( "ADD Cycles: %lu\n", (cnt_after_add - cnt_before_add) );

  /*
   * Benchmark the mul operation
   */
  printf( "Benchmarking MUL\n" );
  clock_gettime( CLOCK_MONOTONIC, &before_mul );
  cnt_before_mul = readtsc();
  for( i=0; i<MAX_AIM9_ITERATIONS; i++ )
    {
      mul_long();
      mul_short();
      mul_int();
    }
  clock_gettime( CLOCK_MONOTONIC, &after_mul );
  cnt_after_mul = readtsc();

  printf( " ==> MUL <== \n" );
  printf( "Time: %d sec \n", (int)(after_mul.tv_sec - before_mul.tv_sec) );
  printf( "MUL Cycles: %lu\n", (cnt_after_mul - cnt_before_mul) );

  /*
   * Benchmark the div operation
   */
  printf( "Benchmarking DIV\n" );
  clock_gettime( CLOCK_MONOTONIC, &before_div );
  cnt_before_div = readtsc();
  for( i=0; i<MAX_AIM9_ITERATIONS; i++ )
    {
      div_long();
      div_short();
      div_int();
    }
  clock_gettime( CLOCK_MONOTONIC, &after_div );
  cnt_after_div = readtsc();

  printf( " ==> DIV <== \n" );
  printf( "Time: %d sec \n", (int)(after_div.tv_sec - before_div.tv_sec) );
  printf( "DIV Cycles: %lu\n", (cnt_after_div - cnt_before_div) );

  cnt_after_global = readtsc();
  clock_gettime( CLOCK_MONOTONIC, &after_global );

  printf( " ==> TOTAL <== \n" );
  printf( "Time: %d sec \t",
	  (int)(after_global.tv_sec-before_global.tv_sec) );
  printf( "Cycles: %lu\n", (cnt_after_global - cnt_before_global) );

  //  time = (cnt_after_global - cnt_before_global);
  //  time = time/3400000000;

  // pes broke
  //  printf("float time %.2lf\n", time);
  return 0;
}
