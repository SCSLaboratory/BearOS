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

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

typedef struct
{
  int quot;
  int rem;
} div_t;

/* used by clock_gettime() */
typedef uint32_t time_t;
typedef int clockid_t;

/*globals */ 
extern uint32_t system_time;

struct timespec {
        time_t tv_sec;    /* seconds */
        long tv_nsec;     /* and nanoseconds */
};

//Functions 
uint32_t init_time();
void read_rtc(struct tm *tp); 
time_t mktime(struct tm *tim_p);
int santiy_check_tm(struct tm *tim_p);
void print_time(struct tm *tim_p);
void localtime(time_t sys_time, struct tm *res);


#define TIME_ZONE 4
#define YEAR_BASE	1900
#define CURRENT_YEAR        2014 
#define EPOCH_YEAR        1970 	
#define EPOCH_YEARS_SINCE_LEAP 2
#define EPOCH_YEARS_SINCE_CENTURY 70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370
#define EPOCH_WDAY      4

#define TIME_MAX  0xffffffffUL

#undef ULONG_MAX	/*  defined previously in usr/include/limits.h */
#define ULONG_MAX 0xffffffffUL 

#undef LONG_MAX 	/* defined previously in usr/include/limits.h */
#define LONG_MAX  2147483647L

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

#define SECSPERMIN	60L
#define MINSPERHOUR	60L
#define HOURSPERDAY	24L
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	(SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK	7
#define MONSPERYEAR	12

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int DAYS_IN_MONTH[12] ={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int DAYS_BEFORE_MONTH[12] ={0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const int LEAP_DAYS_BEFORE_MONTH[12]={0, 31,60,91,121,152,182,213,244,274,304,335}; 
static const int year_lengths[2] = {365,366};

static const int mon_lengths[2][MONSPERYEAR] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
} ;


int days_in_month(int x);


#define _DAYS_IN_MONTH(x) ((x == 1) ? days_in_feb : DAYS_IN_MONTH[x])
#define _ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) (_ISLEAP(year) ? 366 : 365)



