#include "global.h"

/* xQueue handling functions */



void xQueueCreate(Queue *q, int size, void *data)
{
    q->write_index = 0;
    q->read_index = 0;
    q->size = size;
    q->count = 0;
    q->data = data;
}

void    xQueueSendToBack(Queue *q, void *message, int unused_portMAX_DELAY)
{
}

void    xQueueSendToBackFromISR(Queue *q, void *message, int unused_portMAX_DELAY)
{
}

int xQueueReceive(Queue *q, void *message, int unused_portMAX_DELAY )
{
    return 0;
}





