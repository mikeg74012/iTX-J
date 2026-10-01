#include <stdio.h>
#include <string.h>
#include "threads.h"

/* Thread handling */


static int current_thread = 0;
static int num_threads = 1;

extern volatile unsigned long ms_timer;

static thread threads[MAX_THREADS];

extern void hardware_task_switch_begin(volatile unsigned long * new_stack, void(*func)(void));
extern void hardware_task_switch_begin_with(unsigned index, volatile unsigned long * new_stack, void(*func)(unsigned int_param));



void    init_threads (void)
{
    memset (threads, 0, sizeof (threads));

    threads[0].done_first = 1;      /* We don't need to start the first thread 
                                    from scratch as it's the foreground */
}

void    add_thread (void (*start)(void), unsigned long *stack)
{
    thread *t;
    
    if (num_threads >= MAX_THREADS)
    {
        for (;;)
        {
        }
    }

    t = &threads[num_threads];
    
    t->start = start;
    t->stack = stack;
    
    num_threads++;
}


void    sleep (unsigned long ms)
{
    thread *t;
    
    t = &threads[current_thread];
    
    t->sleep_start_time = ms_timer;
    t->sleep_time = ms;
    
    if (!setjmp (t->jmp))
    {
        /* We have stored where we are, so move onto the next */

        for (;;)
        {
            current_thread++;
            
            if (current_thread >= num_threads)
            {
                current_thread = 0;
            }
            
            t = &threads[current_thread];
            
            if (!t->done_first)
            {
                /* This thread hasn't run yet, so execute it now */
                
                break;
            }
 
            /* See if this is sleeping, in which case ignore it */
            
            if (t->sleep_time)
            {
                /* Yes, it is sleeping */
                
                if ((ms_timer - t->sleep_start_time) >= t->sleep_time)
                {
                    /* The sleep time is over, so it needs to execute now */
                    
                    break;
                }
            }
            else
            {
                /* Not sleeping, so always execute */
                
                break;
            }
                
        }
        
        
        if (!t->done_first)
        {
            t->done_first = 1;
    
            /* Switch stack and execute the new thread */
            
            hardware_task_switch_begin(t->stack, t->start);
            
            /* We don't get here.  Or at least I don't think we do */
            
            for (;;)
            {
            }
        }
        else
        {
            longjmp (t->jmp, 1);
        }
        
        /* We don't get here */
    }
}

