/*
 **************************************************************************************************
 * @file   messagequeue.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module handles queueing of messages for sending
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
/* Information pertaining to a given message queue */
typedef struct
{
  int ReadIndex;
  int WriteIndex;
  int MessageCount;
  int StartAddress;
  int MaxRecords;
} QInfo_t;

/* Information pertaining to a given message, reseved with size of  MESSAGE_QUEUE_INFO_SIZE */
typedef struct
{
  uint32_t  Size;
  uint32_t  TimeStamp;
  uint8_t   MessageType;
} MsgInfo_t;


/* Space reserved for message information at start of each record */
#define MESSAGE_QUEUE_INFO_SIZE         32

/* size of each record in the queue */
#define MESSAGE_QUEUE_RECORD_SIZE       (MESSAGE_QUEUE_INFO_SIZE + MESSAGE_MAX_MESSAGE_SIZE)


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static QInfo_t LPQueue;
static QInfo_t HPQueue;


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static void MessageQueueIncrementReadIndex  (QInfo_t * pQueue);
static void MessageQueueIncrementWriteIndex (QInfo_t * pQueue);
static QInfo_t * MessageQueueGetQueue (MessagePriority_e priority);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void MessageQueueInitialise (void)
{
  if (!IsLowPowerSleepFlagUp())
  {
    /* clear the Low Priority queue info */
    LPQueue.MessageCount = 0;
    LPQueue.ReadIndex    = 0;
    LPQueue.WriteIndex   = 0;
    LPQueue.StartAddress = SERIAL_RAM_GPRS_QUEUE_LP_AREA_START;
    LPQueue.MaxRecords   = SERIAL_RAM_GPRS_QUEUE_LP_AREA_SIZE / MESSAGE_QUEUE_RECORD_SIZE;
    TRACE_MESSAGES("Init LP queue: start add = %d, size = %d\n\r", LPQueue.StartAddress, LPQueue.MaxRecords);

    /* clear the High Priority queue info */
    HPQueue.MessageCount = 0;
    HPQueue.ReadIndex    = 0;
    HPQueue.WriteIndex   = 0;
    HPQueue.StartAddress = SERIAL_RAM_GPRS_QUEUE_HP_AREA_START;
    HPQueue.MaxRecords   = SERIAL_RAM_GPRS_QUEUE_HP_AREA_SIZE / MESSAGE_QUEUE_RECORD_SIZE;
    TRACE_MESSAGES("Init HP queue: start add = %d, size = %d\n\r", HPQueue.StartAddress, HPQueue.MaxRecords);
  }
}


/**
 * @brief   Write a Message to the specified queue
 * @param   priority : The queue to write to (high / low priority)
 * @param   pMessage : pointer to the message to add
 * @param   Size     : size of the message in bytes
 * @retval  true on success, false on failure
 */
bool MessageQueueWrite (MessagePriority_e priority, uint8_t * pMessage, int size)
{
  bool result = false;
  MsgInfo_t MsgInfo;

  QInfo_t * pQueue = MessageQueueGetQueue(priority);

  TRACE_MESSAGES("Write message: queue = %d, index = %d, size = %d\n\r", priority, pQueue->WriteIndex, size);

  /* Setup the message info */
  MsgInfo.Size = size;
  MsgInfo.MessageType = MessageGetMessageType(pMessage);
  MsgInfo.TimeStamp = MessageGetMessageTimeStamp(pMessage);

  /* Write the message info header */
  result = SerialRamWrite((pQueue->StartAddress + (pQueue->WriteIndex * MESSAGE_QUEUE_RECORD_SIZE)), (uint8_t *)&MsgInfo, sizeof(MsgInfo_t));

  /* Write the message data */
  if (result)
  {
    result = SerialRamWrite((pQueue->StartAddress + MESSAGE_QUEUE_INFO_SIZE + (pQueue->WriteIndex * MESSAGE_QUEUE_RECORD_SIZE)), pMessage, size);
  }

  /*
   * Increment the write pointer
   * This also takes care of message count moving on read pointer if buffer is full
   */
  if (result)
  {
    MessageQueueIncrementWriteIndex(pQueue);
  }

  if (!result)
  {
    TRACE_MESSAGES("ERROR: Failed to write message\n\r");
  }

  return result;
}


/**
 * @brief   Read a Message from the specified queue.  Note this does NOT remove the message
 *          from the queue.  That must be done with MessageQueueRemove() after receipt is acknowledged
 * @param   priority : The queue to read from (high / low priority)
 * @param   pMessage : pointer to the message to add
 * @param   pSize    : address of storage to put message size into
 * @retval  true on success, false on failure
 */
bool MessageQueueRead (MessagePriority_e priority, uint8_t * pMessage, int * pSize)
{
  bool result = false;
  MsgInfo_t MsgInfo;
  QInfo_t * pQueue = MessageQueueGetQueue(priority);

  TRACE_MESSAGES("Read message: queue = %d, index = %d\n\r", priority, pQueue->ReadIndex);

  if (pQueue->MessageCount)
  {
    /* Read the message info header */
    result = SerialRamRead((pQueue->StartAddress + (pQueue->ReadIndex * MESSAGE_QUEUE_RECORD_SIZE)), (uint8_t *)&MsgInfo, sizeof(MsgInfo_t));

    /* Read the message data */
    if (result)
    {
      if (MsgInfo.Size)
      {
        result = SerialRamRead((pQueue->StartAddress + MESSAGE_QUEUE_INFO_SIZE + (pQueue->ReadIndex * MESSAGE_QUEUE_RECORD_SIZE)), pMessage, MsgInfo.Size);
#if 0
        ConsolePrintf("---------- MESSAGE READ FROM QUEUE ----------\n\r");
        for (int i = 0; i < MsgInfo.Size; i++)
        {
          ConsolePrintf("%02x ", *(pMessage + i));
          if (0 == (i+1) % 16)
          {
            ConsolePrintf("\n\r");
            sleep(5);
          }
        }
        ConsolePrintf("\n\r");
#endif
      }
      else
      {
        TRACE_MESSAGES("ERROR: queue = %d, index = %d, size = 0\n\r", priority, pQueue->ReadIndex);
      }
    }
    if (result)
    {
      *pSize = MsgInfo.Size;
    }
    else
    {
      *pSize = 0;
    }
  }
  /*
   * NOTE: we do not increment the read pointer here.
   * Instead we wait for a call to MessageQueueRemove() once the message has been acknowledged
   */

  if (!result)
  {
    TRACE_MESSAGES("ERROR: Failed to read message\n\r");
  }

  return result;
}



/**
 * @brief   Get the number of messages in the specified queue
 * @param   priority : The queue to check (high / low priority)
 * @retval  int      : the number of messages in the queue
 */
int MessageQueueCount (MessagePriority_e priority)
{
  QInfo_t * pQueue = MessageQueueGetQueue(priority);

  TRACE_MESSAGES("Request Count: queue = %d, count = %d\n\r", priority, pQueue->MessageCount);

  return pQueue->MessageCount;
}


/**
 * @brief   Get the number of messages in queues for the MessageType
 * @param   MessageType : The message type to be checked in the high and low priority queue
 * @retval  int      : the number of messages in the queue with the Message Type
 */
int MessageQueueCountMsgType (MessageType messsageType)
{
  int totalCount = 0;

  for (MessagePriority_e priority = MESSAGE_PRIORITY_LOW; priority < MESSAGE_PRIORITY_MAX; priority++)
  {
    QInfo_t * pQueue = MessageQueueGetQueue(priority);
    if (pQueue == NULL || pQueue->MessageCount == 0)
    {
        // Skip if the queue is empty or invalid
        continue;
    }    
    MsgInfo_t msgInfo;

    /* read the message info header*/
    for (int i=0; i < pQueue->MessageCount; i++)
    {
      // Calculate the current index with wrap-around
      int currentIdx = (pQueue->ReadIndex + i) % pQueue->MaxRecords;

      SerialRamRead((pQueue->StartAddress + (currentIdx * MESSAGE_QUEUE_RECORD_SIZE)), (uint8_t *)&msgInfo, sizeof(MsgInfo_t));
      if ((msgInfo.Size!= 0) && (msgInfo.MessageType == messsageType))
      {
        totalCount ++;
      }
    }
  }
  TRACE_MESSAGES("Request Type = %d, with TotalCount = %d\n\r", messsageType, totalCount);
  return totalCount;  
}


/**
 * @brief   Caluclate the total Journey time of the message that queued in the Message Queue (in seconds)
 * @param   none
 * @retval  int      : the total Journey time of the message that queued in the Message Queue (in seconds)
 */
int MessageQueueGetTotalJourneyDuration (void)
{
  int totalDuration  = 0;
  int totalJourneyCount = 0;

  for (MessagePriority_e priority = MESSAGE_PRIORITY_LOW; priority < MESSAGE_PRIORITY_MAX; priority++)
  {
    QInfo_t * pQueue = MessageQueueGetQueue(priority);
    if (pQueue == NULL || pQueue->MessageCount == 0)
    {
        // Skip if the queue is empty or invalid
        continue;
    }
    MsgInfo_t msgInfo;
    int startTime = 0;

    /* read the message info header */
    for (int i=0; i < pQueue->MessageCount; i++)
    {
      // Calculate the current index with wrap-around
      int currentIdx = (pQueue->ReadIndex + i) % pQueue->MaxRecords;

      SerialRamRead((pQueue->StartAddress + (currentIdx * MESSAGE_QUEUE_RECORD_SIZE)), (uint8_t *)&msgInfo, sizeof(MsgInfo_t));
      if (msgInfo.Size == 0)
      {
          // Skip empty or invalid messages
          continue;
      }      
      if (msgInfo.MessageType == MESSAGE_TYPE_START_OF_JOURNEY)
      {
        startTime = msgInfo.TimeStamp;
        totalJourneyCount++;
      }
      else if (msgInfo.MessageType == MESSAGE_TYPE_END_OF_JOURNEY)
      {
        if (startTime != 0 && msgInfo.TimeStamp != 0)
        {
          totalDuration += msgInfo.TimeStamp - startTime;
          startTime = 0;
        }
      }
    }
  }
  TRACE_MESSAGES("Total Journey: %d, with duration = %d\n\r", totalJourneyCount, totalDuration);
  return totalDuration;  
}


/**
 * @brief   Remove the last Read packet from the queue
 * @param   priority : The queue to check (high / low priority)
 * @retval  bool     : true on success, false on failure
 */
int MessageQueueRemove (MessagePriority_e priority)
{
  QInfo_t * pQueue = MessageQueueGetQueue(priority);
  MsgInfo_t MsgInfo;

  TRACE_MESSAGES("Remove message: queue = %d, index = %d\n\r", priority, pQueue->ReadIndex);

  /* Zeroed out the message info for the init */
  memset(&MsgInfo, 0, sizeof(MsgInfo));

  /* Write the zeroed out data into the info header */
  bool result = SerialRamWrite((pQueue->StartAddress + (pQueue->ReadIndex * MESSAGE_QUEUE_RECORD_SIZE)), (uint8_t *)&MsgInfo, sizeof(MsgInfo_t));

  /* increment the read pointer */
  MessageQueueIncrementReadIndex(pQueue);

  if (!result)
  {
    TRACE_MESSAGES("ERROR - failed to remove message\n\r");
  }
  return result;
}


/**
 * @brief   return LP queue Info address
 * @param   none
 * @retval  pointer to the LPQueue.
 */
uint8_t * GetMessageQueueLPPoint (void)
{
  return ((uint8_t *) &LPQueue);
}


/**
 * @brief   return HP queue Info address
 * @param   none
 * @retval  pointer to the HPQueue.
 */
uint8_t * GetMessageQueueHPPoint (void)
{
  return ((uint8_t *) &HPQueue);
}


/**
 * @brief   return size of QInfo_t structure
 * @param   none
 * @retval  size of QInfo_t.
 */
uint8_t GetMessageQueueInfoSize (void)
{
  return ((uint8_t)sizeof(QInfo_t));
}


/**
 * @brief   return current Queued message count
 * @param   queueID: 0 for LP and 1 for HP
 * @retval  Queued MSg count.
 */
int GetMessageQueueCount (uint8_t qID)
{
  return (qID? HPQueue.MessageCount: LPQueue.MessageCount);
}
/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     Return pointer to the requested queue object
 * @param     pQueue : pointer to queue object to manipulate
 * @return    none
 */
static QInfo_t * MessageQueueGetQueue (MessagePriority_e priority)
{
  if (MESSAGE_PRIORITY_LOW == priority)
  {
    return &LPQueue;
  }
  return &HPQueue;
}


/**
 * @brief     Increment the READ index of the queue object passed, wrapping if end of buffer is reached
 * @param     pQueue : pointer to queue object to manipulate
 * @return    none
 */
static void MessageQueueIncrementReadIndex (QInfo_t * pQueue)
{
  if (pQueue->ReadIndex != pQueue->WriteIndex)
  {
    pQueue->ReadIndex++;

    if (pQueue->ReadIndex >= pQueue->MaxRecords)
    {
      pQueue->ReadIndex = 0;
    }
    pQueue->MessageCount--;
  }
  else
  {
    /* should not be needed, but added for safety */
    pQueue->MessageCount = 0;
  }
}


/**
 * @brief     Increment the WRITE index of the queue object passed, wrapping if end of buffer is reached.
 *            Also move the read index if the buffer is full (write must not overtake read, so we lose oldest data)
 * @param     pQueue : pointer to queue object to manipulate
 * @return    none
 */
static void MessageQueueIncrementWriteIndex (QInfo_t * pQueue)
{
  pQueue->WriteIndex++;

  if (pQueue->WriteIndex >= pQueue->MaxRecords)
  {
    pQueue->WriteIndex = 0;
  }
  if (pQueue->WriteIndex == pQueue->ReadIndex)
  {
    pQueue->ReadIndex++;

    if (pQueue->ReadIndex >= pQueue->MaxRecords)
    {
      pQueue->ReadIndex = 0;
    }
  }
  else
  {
    pQueue->MessageCount++;
  }
}



/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       list the message queue count with type
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MessageQueueConsoleList (int ArgCount, char **pArguments)
{
  int msgType;

  if (1 == ArgCount)
  {
    /* get the msg type from the command line */
    sscanf(pArguments[0],  "%d", &msgType);
    int cnt = MessageQueueCountMsgType(msgType);

    ConsolePrintf("Message Type with %d cnt: %d\r\n", msgType, cnt);
    return (CONSOLE_ERROR_OK);
  }
  else
  {
    ConsolePrintf("Total Journeys In Queue with duration %ds\r\n", MessageQueueGetTotalJourneyDuration());
    return (CONSOLE_ERROR_OK);
  }

  return (CONSOLE_ARG_NO);
}