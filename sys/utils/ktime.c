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

/******************************************************************************
 * Filename: time.c
 *
 * Description: 
 * Retreives the wall clock time from the system real time clock RTC
 * FIXME No daylight savings time!  
 * TODO This can only read the system time, there is no provision to set it
 ****************************************************************************/
#include <stdint.h>
#include <ktime.h>
#include <pio.h>
#include <constants.h>
#include <ktimer.h>
#include <kstdio.h>

int century_register = 0x32;  
uint32_t system_time =0; 

void NMI_enable(void){
  outb(0x70, inb(0x70)&0x7F);
}
 
void NMI_disable(void){
  outb(0x70, inb(0x70)|0x80);
}

enum {
  cmos_address = 0x70,
  cmos_data    = 0x71
};

unsigned char get_RTC_register(int reg) {
  outb(cmos_address, reg);
  return inb(cmos_data);
}

int get_update_in_progress_flag() {
  outb(cmos_address, 0x0A);
  return (inb(cmos_data) & 0x80);
}

int santiy_check_tm(struct tm *tim_p){

  if (tim_p->tm_sec < 0 || tim_p->tm_sec > 59)
    return 1;
  if (tim_p->tm_min < 0 || tim_p->tm_min > 59)
    return 2;
  if (tim_p->tm_hour < 0 || tim_p->tm_hour > 23)
    return 3;
  if (tim_p->tm_mon < 0 || tim_p->tm_mon > 12)
    return 4;
  if (tim_p->tm_mday > DAYS_IN_MONTH[tim_p->tm_mon-1] ||  tim_p->tm_mday < 0 )
    return 5;

  return 0;
}

static void wall_clock_tick(){
  system_time+=1;

  /*Reregister the alarm */
  ktimer_new_alarm(1000, &wall_clock_tick, NULL);
 
}

uint32_t init_time(){
  int i;	
  struct tm tm_now;
  read_rtc(&tm_now);
  if( (i = santiy_check_tm(&tm_now)) ){
    kprintf("WARNING system time read RTC failure, %d", i);
    return 0; 
  }
  system_time = mktime(&tm_now);
  ktimer_new_alarm(1000, &wall_clock_tick, NULL);
  return system_time;
}

void read_rtc(struct tm *tp) {
  unsigned char century;
  unsigned char last_second;
  unsigned char last_minute;
  unsigned char last_hour;
  unsigned char last_day;
  unsigned char last_month;
  unsigned char last_year;
  unsigned char last_century;
  unsigned char registerB;
  unsigned char second;
  unsigned char minute;
  unsigned char hour;
  unsigned char day;
  unsigned char month;
  unsigned int year;
  unsigned int days=0; 

  // Note: This uses the "read registers until you get the same values twice in a row" technique
  //       to avoid getting dodgy/inconsistent values due to RTC updates
  NMI_disable();	 

  while (get_update_in_progress_flag());                // Make sure an update isn't in progress
  second = get_RTC_register(0x00);
  minute = get_RTC_register(0x02);
  hour = get_RTC_register(0x04);
  day = get_RTC_register(0x07);
  month = get_RTC_register(0x08);
  year = get_RTC_register(0x09);
  if(century_register != 0) {
    century = get_RTC_register(century_register);
  }

  do {
    last_second = second;
    last_minute = minute;
    last_hour = hour;
    last_day = day;
    last_month = month;
    last_year = year;
    last_century = century;
    while (get_update_in_progress_flag());  // Make sure an update isn't in progress
	 
    second = get_RTC_register(0x00);
    minute = get_RTC_register(0x02);
    hour = get_RTC_register(0x04);
    day = get_RTC_register(0x07);
    month = get_RTC_register(0x08);
    year = get_RTC_register(0x09);
    if(century_register != 0) {
      century = get_RTC_register(century_register);
    }
  } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
	   (last_day != day) || (last_month != month) || (last_year != year) ||
	   (last_century != century) );
 
  registerB = get_RTC_register(0x0B);
 
  // Convert BCD to binary values if necessary
  if (!(registerB & 0x04)) {
    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
    day = (day & 0x0F) + ((day / 16) * 10);
    month = (month & 0x0F) + ((month / 16) * 10);
    year = (year & 0x0F) + ((year / 16) * 10);
    if(century_register != 0) {
      century = (century & 0x0F) + ((century / 16) * 10);
    }
  }
 
  // Convert 12 hour clock to 24 hour clock if necessary
  if (!(registerB & 0x02) && (hour & 0x80)) {
    hour = ((hour & 0x7F) + 12) % 24;
  }
 
  // Calculate the full (4-digit) year
  if(century_register != 0) {
    year += century * 100;
  }
  else {
    year += (CURRENT_YEAR / 100) * 100;
    if(year < CURRENT_YEAR) year += 100;
  }

  tp->tm_sec = second;
  //kprintf("sec %d\n",second);
  tp -> tm_min = minute;
  //kprintf("min %d\n",minute);
  tp -> tm_hour = hour;
  //kprintf("hour %d\n",hour);
  tp -> tm_mday = day;
  //kprintf("DEBUG day %d",day);
  tp -> tm_mon = month;
  //kprintf("month %d ",month);
  tp -> tm_year = year;
  //kprintf("year %d\n ",year);

  /*calculate the days in this year so far */
  days += day;
  if(isleap(year))
    days += LEAP_DAYS_BEFORE_MONTH[month-1];
  else
    days += DAYS_BEFORE_MONTH[month-1];
	  
  tp->tm_yday = days;

  tp->tm_wday = 0;
  tp->tm_isdst = 0;

  NMI_enable();
}
//////////////////divide TODO move me somewhere generic!!! /////////////
div_t div(num, denom){

  div_t r;

  r.quot = num / denom;
  r.rem = num % denom;
  /*
   * The ANSI standard says that |r.quot| <= |n/d|, where
   * n/d is to be computed in infinite precision.  In other
   * words, we should always truncate the quotient towards
   * 0, never -infinity or +infinity.
   *
   * Machine division and remainer may work either way when
   * one or both of n or d is negative.  If only one is
   * negative and r.quot has been truncated towards -inf,
   * r.rem will have the same sign as denom and the opposite
   * sign of num; if both are negative and r.quot has been
   * truncated towards -inf, r.rem will be positive (will
   * have the opposite sign of num).  These are considered
   * `wrong'.
   *
   * If both are num and denom are positive, r will always
   * be positive.
   *
   * This all boils down to:
   *      if num >= 0, but r.rem < 0, we got the wrong answer.
   * In that case, to get the right answer, add 1 to r.quot and
   * subtract denom from r.rem.
   *      if num < 0, but r.rem > 0, we also have the wrong answer.
   * In this case, to get the right answer, subtract 1 from r.quot and
   * add denom to r.rem.
   */
  if (num >= 0 && r.rem < 0) {
    ++r.quot;
    r.rem -= denom;
  }
  else if (num < 0 && r.rem > 0) {
    --r.quot;
    r.rem += denom;
  }
  return (r);
}

static void validate_structure(struct tm *tim_p)
{
  div_t res;
  int days_in_feb = 28;


  /* calculate time & date to account for out of range values */
  if (tim_p->tm_sec < 0 || tim_p->tm_sec > 59)
    {
      res = div (tim_p->tm_sec, 60);
      tim_p->tm_min += res.quot;
      if ((tim_p->tm_sec = res.rem) < 0)
	{
	  tim_p->tm_sec += 60;
	  --tim_p->tm_min;
	}
    }

  if (tim_p->tm_min < 0 || tim_p->tm_min > 59)
    {
      res = div (tim_p->tm_min, 60);
      tim_p->tm_hour += res.quot;
      if ((tim_p->tm_min = res.rem) < 0)
	{
	  tim_p->tm_min += 60;
	  --tim_p->tm_hour;
        }
    }

  if (tim_p->tm_hour < 0 || tim_p->tm_hour > 23)
    {
      res = div (tim_p->tm_hour, 24);
      tim_p->tm_mday += res.quot;
      if ((tim_p->tm_hour = res.rem) < 0)
	{
	  tim_p->tm_hour += 24;
	  --tim_p->tm_mday;
        }
    }

  if (tim_p->tm_mon < 0 || tim_p->tm_mon > 11)
    {
      res = div (tim_p->tm_mon, 12);
      tim_p->tm_year += res.quot;
      if ((tim_p->tm_mon = res.rem) < 0)
        {
	  tim_p->tm_mon += 12;
	  --tim_p->tm_year;
        }
    }

  if (_DAYS_IN_YEAR (tim_p->tm_year) == 366)
    days_in_feb = 29;

  if (tim_p->tm_mday <= 0)
    {
      while (tim_p->tm_mday <= 0)
	{
	  if (--tim_p->tm_mon == -1)
	    {
	      tim_p->tm_year--;
	      tim_p->tm_mon = 11;
	      days_in_feb =
		((_DAYS_IN_YEAR (tim_p->tm_year) == 366) ?
		 29 : 28);
	    }
	  tim_p->tm_mday += _DAYS_IN_MONTH (tim_p->tm_mon);
	}
    }
  else
    {
      while (tim_p->tm_mday > _DAYS_IN_MONTH (tim_p->tm_mon))
	{
	  tim_p->tm_mday -= _DAYS_IN_MONTH (tim_p->tm_mon);
	  if (++tim_p->tm_mon == 12)
	    {
	      tim_p->tm_year++;
	      tim_p->tm_mon = 0;
	      days_in_feb =
		((_DAYS_IN_YEAR (tim_p->tm_year) == 366) ?
		 29 : 28);
	    }
	}
    }
}


void print_time(struct tm *tim_p){

  if(tim_p != NULL){
    kprintf("\nhour %d, min %d, sec %d \n", tim_p->tm_hour, tim_p->tm_min, tim_p->tm_sec);
    kprintf("year %d, month %d, day %d\n", tim_p->tm_year, tim_p->tm_mon, tim_p->tm_mday);	
    kprintf("week day %d, year day %d, is dst %d",tim_p->tm_wday, tim_p->tm_yday, tim_p->tm_isdst);
  }
}


time_t mktime(struct tm *tim_p)
{
  time_t tim = 0;
  long days = 0;
  int year;
	
  /* validate structure */
  validate_structure (tim_p);

  /* compute hours, minutes, seconds */
  tim += tim_p->tm_sec + (tim_p->tm_min * _SEC_IN_MINUTE) +
    (tim_p->tm_hour * _SEC_IN_HOUR);

  /* compute days in  this year */
  days += tim_p->tm_mday;
  tim_p->tm_mon = tim_p->tm_mon -1;
  days += DAYS_BEFORE_MONTH[tim_p->tm_mon];
  if (tim_p->tm_mon > 1 && _DAYS_IN_YEAR (tim_p->tm_year) == 366)
    days++;

  /* set day of the year */
  tim_p->tm_yday = days;

  if (tim_p->tm_year > 10000 || tim_p->tm_year < -10000)
    return (time_t) -1;

  year = tim_p->tm_year - 1;

  do{
    days += _DAYS_IN_YEAR (year);
    year--;
  }while(year >= EPOCH_YEAR);

  //kprintf("days total %d", days);

  /* compute total seconds */
  tim += (days * _SEC_IN_DAY);

  /* compute day of the week */
  if ((tim_p->tm_wday = (days + 4) % 7) < 0)
    tim_p->tm_wday += 7;

  return tim;
}

void localtime(time_t sys_time, struct tm *res)
{
  long days, rem;
  time_t lcltime;
  int y;
  int yleap;
  const int *ip;

  lcltime = sys_time;
   
  days = ((long)lcltime) / SECSPERDAY;
  rem = ((long)lcltime) % SECSPERDAY;
  while (rem < 0) 
    {
      rem += SECSPERDAY;
      --days;
    }
  while (rem >= SECSPERDAY)
    {
      rem -= SECSPERDAY;
      ++days;
    }
 
  /* compute hour, min, and sec */  
  res->tm_hour = (int) (rem / SECSPERHOUR);
  rem %= SECSPERHOUR;
  res->tm_min = (int) (rem / SECSPERMIN);
  res->tm_sec = (int) (rem % SECSPERMIN);

  /* compute day of week */
  if ((res->tm_wday = ((EPOCH_WDAY + days) % DAYSPERWEEK)) < 0)
    res->tm_wday += DAYSPERWEEK;

  /* compute year & day of year */
  y = EPOCH_YEAR;
  if (days >= 0){
    for (;;){
      yleap = isleap(y);
      if (days < year_lengths[yleap])
	break;
      y++;
      days -= year_lengths[yleap];
    }
  }
  else{
    do{
      --y;
      yleap = isleap(y);
      days += year_lengths[yleap];
    } while (days < 0);
  }
  res->tm_yday = days+1;
  res->tm_year = y;
  
  ip = mon_lengths[yleap];
  for (res->tm_mon = 0; days >= ip[res->tm_mon]; ++(res->tm_mon))
    days -= ip[res->tm_mon];
  res->tm_mon +=1;	
  res->tm_mday = days + 1;

  res->tm_isdst = 0;

}


