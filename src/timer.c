#include "timer.h"

double timer_diff(timer *time)
{ 
  struct timeval diff;

  diff.tv_sec  = time->stop.tv_sec - time->start.tv_sec ;
  diff.tv_usec = time->stop.tv_usec - time->start.tv_usec;

  return (double)(diff.tv_sec + (double)diff.tv_usec / (double)1000000); /* freebsd CLOCKS_PER_SEC = 128 ?! */
}

void timer_start(timer *time)
{
    gettimeofday(&time->start, NULL);
}

void timer_stop(timer *time)
{
    gettimeofday(&time->stop, NULL);
}

/* made april fool's day :) */
void time2human(double seconds)
{
    int value = 0, i;
    static int conv[] = {31536000, 86400, 3600, 60, 0, -1};
    static char* name[] = {"year", "day", "hour", "minute", "second", 0};

    if (seconds < 0)
    return;

    for(i = 0; conv[i] >= 0 ; i++)
    {
        if (conv[i])
            value = (((uint32_t)seconds) / conv[i]);
        if (value || !conv[i])
        {
            if (conv[i])
                printf("%d %s", value, name[i]);
            else
            {
                printf("%g %s", seconds, name[i]);
                if (seconds != 1)
                    printf("s");
            }

            if (value > 1 && conv[i])
                printf("s");

            printf(" ");
            seconds -= conv[i] * value; /* I don't like mod */
        }
    }
    printf("\n");
}

