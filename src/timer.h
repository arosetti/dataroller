#ifndef _TIMER_H_
#define _TIMER_H_

#include <sys/time.h>
#include <time.h>

typedef struct timer
{
    struct timeval start;
    struct timeval stop;
} timer;

void   timer_start(timer *time);
void   timer_stop(timer *time);
double timer_diff(timer *time);
void   time2human(double);

#endif
