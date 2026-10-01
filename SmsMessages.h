/*
 **************************************************************************************************
 * @file   smsmessages.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module encodes and decodes messages used for SMS communications
 *
 **************************************************************************************************
 */

#ifndef _SMS_MESSAGES_H_
#define _SMS_MESSAGES_H_

#include <stdbool.h>
#include "SeqEmergency.h"


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* SMS Configuration */
#define MAX_SMS_LENGTH             160 // 7-bit char format
#define SMS_CRC_LENGTH             4
#define SMS_MESSAGE_TYPE_IDX       0

/* SMS message type ID's */
#define SMS_MESSAGE_TYPE_CONFIG          'C'
#define SMS_MESSAGE_TYPE_STATUS_REQ      'S'
#define SMS_MESSAGE_TYPE_STATUS_INFO     'S'
#define SMS_MESSAGE_TYPE_EMERGENCY       'I'
#define SMS_MESSAGE_TYPE_EMERGENCY_RED   'R'
#define SMS_MESSAGE_TYPE_INIITIALIZATION 'Z'
#define SMS_MESSAGE_TYPE_STATUS_GSM      'G'
#define SMS_MESSAGE_TYPE_CONFIG_REQ      'V'
#define SMS_MESSAGE_TYPE_CONFIG_INFO     'V'


/* SMS Message Version */
#define SMS_MSG_VERSION_CHAR_1           '0'
#define SMS_MSG_VERSION_CHAR_2           '1'


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void MessageEncodeSmsGim (uint8_t ** ppMessage, int * pMessageSize);
void MessageEncodeSmsDim (uint8_t ** ppMessage, int * pMessageSize);
void MessageEncodeSmsCim (uint8_t ** ppMessage, int * pMessageSize, uint8_t blockType, uint8_t blockLength, char * pBlockData);
void MessageEncodeSmsSim (uint8_t ** ppMessage, int * pMessageSize);
void MessageEncodeSmsEIM (uint8_t ** ppMessage, int * pMessageSize, eEmergencyType messageType, bool gprsTxed);

#endif /* _MESSAGE_H_ */
