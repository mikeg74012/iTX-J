#include "global.h"

/* This functions handles the xTimer functions that were in the RTOS */

TimerHandle_t *xTimerCreate(unsigned long period_ms, void (* WatchDogTimerCallback )(TimerHandle_t *timer))
{
    return NULL;
}

void  xTimerStart(TimerHandle_t *t,  unsigned long unused_block_time)
{
}

void  xTimerStop(TimerHandle_t *t,  unsigned long unused_block_time)
{
}

void  xTimerDelete(TimerHandle_t *t,  unsigned long unused_block_time)
{
}

void    xTimerChangePeriod( TimerHandle_t *t, unsigned long period_ms, unsigned long unused_block_time)
{
}



