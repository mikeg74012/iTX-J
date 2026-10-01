/*
 **************************************************************************************************
 * @file   messages.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module encodes and decodes messages used for cloud communications
 *
 **************************************************************************************************
 */

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Debug trace for GPRS message modules */
#if TRACE_MESSAGES_ENABLE
  #define TRACE_MESSAGES(...) TRACE_COLOUR_(FG_BLACK, BG_WHITE, "MESS", __VA_ARGS__)
#else
  #define TRACE_MESSAGES(...)
#endif


/* Maximum allowed message size in bytes */
#define MESSAGE_MAX_MESSAGE_SIZE     1240

/* cloud message type ID's */
typedef enum {
  MESSAGE_TYPE_INITIAISATION = 1,
  MESSAGE_TYPE_HEARTBEAT,
  MESSAGE_TYPE_START_OF_JOURNEY,
  MESSAGE_TYPE_PERIODIC_JOURNEY,
  MESSAGE_TYPE_END_OF_JOURNEY,
  MESSAGE_TYPE_IMPACT,
  MESSAGE_TYPE_MINI_EVENT,
  MESSAGE_TYPE_REDBUTTON,
  MESSAGE_TYPE_STATUS,
  MESSAGE_TYPE_RADIOSCAN,
  MESSAGE_TYPE_CONFIG_VALUE,
  MESSAGE_TYPE_INVALID  = 0xFF
} MessageType;


/* FLags to indicate different types of journey message */
#define PJM_FLAGS_NONE                 0
#define PJM_FLAGS_EOJ                  1


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool MessageEnqueueDim (void);
bool MessageEnqueueDhm (void);
bool MessageEnqueueDsm (void);
bool MessageEnqueueIem (void);
bool MessageEnqueueMem (void);
bool MessageEnqueueRbm (void);
bool MessageEnqueueRsm (void);
bool MessageEnqueueCim (void);
void MessageLPSSetPacketCount     (uint32_t pCount);
bool MessageEnqueueJourneyMessage (MessageType journeyType);
uint32_t MessageLPSGetPacketCount (void);
uint8_t  MessageGetIDs            (uint8_t idType);
void     MessageRestoreIDs        (uint8_t idType, uint8_t idValue);
MessageType  MessageGetMessageType      (uint8_t * pMessage);
uint32_t     MessageGetMessageTimeStamp (uint8_t * pMessage);

#endif /* _MESSAGE_H_ */
