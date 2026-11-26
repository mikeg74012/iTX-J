#ifndef __xqueue_h__
#define __xqueue_h__


typedef struct
{
    int msgId;
    void *parameter;
} QueueMessage;

typedef struct
{
    int write_index;
    int read_index;
    int count;
    int size;
    QueueMessage *data;
} Queue;

void xQueueCreate(Queue *q, int size, void *data);
void    xQueueSendToBack(Queue *q, void *message, int unused_portMAX_DELAY);
void    xQueueSendToBackFromISR(Queue *q, void *message, int unused_portMAX_DELAY);
int xQueueReceive(Queue *q, void *message, int unused_portMAX_DELAY );

#endif
