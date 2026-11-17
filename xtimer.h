#ifndef __xtimer_h__
#define __xtimer_h__

#define portMAX_DELAY   0xffffffff

typedef struct
{
    int reserved;
} TimerHandle_t;


TimerHandle_t *xTimerCreate(unsigned long period_ms, void (*WatchDogTimerCallback)(TimerHandle_t *timer));
void  xTimerStart(TimerHandle_t *t,  unsigned long unused_block_time);
void  xTimerStop(TimerHandle_t *t,  unsigned long unused_block_time);
void  xTimerDelete(TimerHandle_t *t,  unsigned long unused_block_time);
void    xTimerChangePeriod( TimerHandle_t *t, unsigned long period_ms, unsigned long unused_block_time);

#ifdef __xtimer_c__
#define _SCOPE
#else
#define _SCOPE extern
#endif

_SCOPE   volatile unsigned long ms_timer;

#endif
