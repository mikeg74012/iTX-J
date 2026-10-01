/*
 **************************************************************************************************
 * @file   messagequeue.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module handles queueing of messages for sending
 *
 **************************************************************************************************
 */

#ifndef MESSAGES_MESSAGEQUEUE_H_
#define MESSAGES_MESSAGEQUEUE_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

/*
 * Cloud Message Queues
 * Used to queue messages to be sent over GPRS to the cloud server
 * Two queues are defined, to allow emergency messages to take priority
 */
typedef enum
{
  MESSAGE_PRIORITY_LOW,      // low priority queue for most messages
  MESSAGE_PRIORITY_HIGH,     // high priority queue for impact and red-button messages
  MESSAGE_PRIORITY_MAX,
} MessagePriority_e;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
void MessageQueueInitialise   (void);
bool MessageQueueWrite        (MessagePriority_e priority, uint8_t * pMessage, int size);
bool MessageQueueRead         (MessagePriority_e priority, uint8_t * pMessage, int * pSize);
int  MessageQueueRemove       (MessagePriority_e priority);
int  MessageQueueCount        (MessagePriority_e priority);
int  MessageQueueCountMsgType (MessageType mesageType);
int  MessageQueueGetTotalJourneyDuration   (void);

uint8_t *  GetMessageQueueLPPoint    (void);
uint8_t *  GetMessageQueueHPPoint    (void);
uint8_t    GetMessageQueueInfoSize   (void);
int        GetMessageQueueCount      (uint8_t qID);

int        MessageQueueConsoleList   (int ArgCount, char **pArguments);

#endif /* MESSAGES_MESSAGEQUEUE_H_ */
