#ifndef __threads_h__
#define __threads_h__

#include <setjmp.h>

#define MAX_THREADS     10

typedef struct
{
    jmp_buf jmp;
    unsigned char done_first;
    void(*start)(void);
    unsigned long *stack;
    unsigned long sleep_start_time;
    unsigned long sleep_time;
} thread;



void    add_thread (void (*start)(void), unsigned long *stack);
void    sleep (unsigned long ms);
void    init_threads (void);



#endif

