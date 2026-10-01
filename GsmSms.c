/**
 **************************************************************************************************
 * @file   gsmSms.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements SMS functionality for GSM Modems.
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */



/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static char AtCommand[MAX_CONSOLE_AT_LENGTH];


/*
 **************************************************************************************************
 *                                    EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern bool smsStored;
extern char textSmsReceived[MAX_SMS_LEN];



/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool GsmSmsConfigure                       (void);
static bool GsmSmsSendMessage                     (char * pNumber, char * pMessage);
static bool GsmSmsGetRxList                       (void);
static bool GsmSmsDeleteAllMessages               (void);
static bool GsmSmsProcesReceivedSms               (void);
static void GsmSmsHandleStatusRequestMessage      (char *smsString);
static void GsmSmsHandleConfigValueRequestMessage (char *smsString);

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Send an SMS message
 * @param   pNumber - destination number, null terminated string
 * @param   pMessage - data to send, null terminated string
 * @retval  true on success, false on failure
 */
bool GsmSmsSend (char * pNumber, char * pMessage)
{
  TRACE_MODEM("SMS to %s : \"%s\"\n\r", pNumber, pMessage);

  /* configure the SMS message format and handling */
  bool result = GsmSmsConfigure();

  /* Send the message */
  if (result)
  {
    result = GsmSmsSendMessage(pNumber, pMessage);
  }
  return result;
}


/**
 * @brief   Check for any received messages.  Decode them if available
 * @param   none
 * @retval  true on success, false on failure
 */
bool GsmSmsReceive (void)
{
  bool SmsParsedOK = false;

  TRACE_MODEM("Check for received SMS\n\r");

  /* configure the SMS message format and handling */
  bool result = GsmSmsConfigure();

  /* check for received messages */
  if (result)
  {
    result = GsmSmsGetRxList();
  }

  /* Decode any available messages */
  if ((result) && (smsStored))
  {
    /*
     * Delete messages from modem / SIM
     * NOTE: We do this BEFORE we process the message, because reset message is acted upon instantly.
     */
    if (result)
    {
      TRACE_MODEM("Delete received SMS\n\r");
      result = GsmSmsDeleteAllMessages();
    }

    /* now process the received message */
    TRACE_MODEM("Process received SMS\n\r");
    smsStored = false;
    SmsParsedOK = GsmSmsProcesReceivedSms();

    /* Send acknowledgement SMS (if enabled in config store) */
    if (ConfigGetFlag(CONFIG_FLAG_SMS_ACK))
    {
      result = GsmSmsSendMessage(ConfigGetSmsNumber(), SmsParsedOK ? "ACK" : "NAK");
    }
  }

  return result;
}



/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief  Set the SMS format, character set and headers
 * @param  none
 * @retval true on success, false on failure
 */
static bool GsmSmsConfigure (void)
{
  /* Set the SMS format to text (not PDU) */
  eAtResult AtResult = gsmCommsSendAtCommand("AT+CMGF=1", DEFAULT_AT_TIMEOUT_MS);

  /* Set the SMS character set to GSM */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendAtCommand("AT+CSCS=\"GSM\"", DEFAULT_AT_TIMEOUT_MS);
  }

  /* Don't show message headers */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendAtCommand("AT+CSDH=0", DEFAULT_AT_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Send the message via SMS
 * @param  pNumber - international format phone number, null terminated string
 * @param  pMessage - the message data to send, null terminated string
 * @retval true on success, false on failure
 */
static bool GsmSmsSendMessage (char * pNumber, char * pMessage)
{
  eAtResult AtResult;

  /* Build the AT command in format: AT+CMGS="InternationalNumber" and send to the modem */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+CMGS=\"%s\"", pNumber);
  AtResult = gsmCommsSendAtCommand(AtCommand, 30000);

  /* Send the message data */
  if (AT_READY_TO_SEND == AtResult)
  {
      AtResult = gsmCommsSendString(pMessage, 30000);
  }

  if (AT_COMPLETE == AtResult)
  {
    TRACE_MODEM("SMS sent successfully\n\r");
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Request a list of unread received SMS messages from the modem
 * @param  none
 * @retval true on success, false on failure
 */
static bool GsmSmsGetRxList (void)
{
  eAtResult AtResult = gsmCommsSendAtCommand("AT+CMGL=\"REC UNREAD\"", 5000);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Delete all SMS messages from the modem / SIM
 * @param  none
 * @retval true on success, false on failure
 */
static bool GsmSmsDeleteAllMessages (void)
{
  eAtResult AtResult = gsmCommsSendAtCommand("AT+QMGDA=\"DEL ALL\"", 3000);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
  * @brief    Process a received SMS and forward to the correct module for processing
  * @Warning: Currently, all incoming SMS are sent to config module
  * @param    None
  * @retval   None
  */
static bool GsmSmsProcesReceivedSms (void)
{
  bool parsedOK = false;

  TRACE_MODEM("Received SMS: %s\n\r", textSmsReceived);

  /* Parse SMS */
  uint16_t stringLength = strnlen(textSmsReceived, MAX_SMS_LENGTH);

  if ((int16_t)(stringLength-SMS_CRC_LENGTH) > 0)
  {
    /* Check the CRC */
    unsigned int readCRC;
    if (sscanf(&textSmsReceived[stringLength-SMS_CRC_LENGTH], "%4X", &readCRC) == 1)
    {
      uint16_t calcCRC = CrcDriverCalculateCrc16((uint8_t *)textSmsReceived, stringLength-SMS_CRC_LENGTH);

      /* Calculate CRC */
      TRACE_MODEM("SMS Read CRC = %4X, SMS Expected CRC = %4X\n\r", readCRC, calcCRC);

      if ((uint16_t) readCRC == calcCRC)
      {
        parsedOK = true;

        /* Process incoming SMS dependent on message type field */
        switch(textSmsReceived[SMS_MESSAGE_TYPE_IDX])
        {
          /* SMS Configuration Request Message (CRM) */
          case SMS_MESSAGE_TYPE_CONFIG:
            (void) ConfigHandleViaSMS(&textSmsReceived[SMS_MESSAGE_TYPE_IDX+1]);
            break;
          /* SMS Status Request Message (SRM) */
          case SMS_MESSAGE_TYPE_STATUS_REQ:
            (void) GsmSmsHandleStatusRequestMessage(&textSmsReceived[SMS_MESSAGE_TYPE_IDX+1]);
            break;
          case SMS_MESSAGE_TYPE_CONFIG_REQ:
            (void) GsmSmsHandleConfigValueRequestMessage(&textSmsReceived[SMS_MESSAGE_TYPE_IDX+1]);
            break;
          default:
            TRACE_MODEM("Error: Incoming SMS type unknown!!!\n\r");
            break;
        }
      }
      else
      {
        TRACE_MODEM("Error: Received SMS CRC failed!!\n\r");
      }
    }
    else
    {
      TRACE_MODEM("Error reading SMS CRC field\n\r");
    }
  }
  else
  {
    TRACE_MODEM("SMS CRC Length error (msg length = %d)\n\r", (int16_t)(stringLength-SMS_CRC_LENGTH));
  }
  return parsedOK;
}


/**
 * @brief     Process the incoming SMS Status Request Message
 * @param     The SMS string
 * @return    none
 */
static void GsmSmsHandleStatusRequestMessage (char *smsString)
{
  TRACE_MODEM("SMS Status Request Message type Received\n\r");

  /* Parse SRM Data format */
  if (strnlen(smsString, MAX_SMS_LENGTH) > 2)
  {
    /* Check version chars */
    if ((smsString[0] == SMS_MSG_VERSION_CHAR_1) && (smsString[1] == SMS_MSG_VERSION_CHAR_2))
    {
      /* Encode and Send Status Request message via SMS */
      TRACE_MODEM("Send SMS Device Status Information message summary\n\r");

      uint8_t * pMessage = NULL;
      int MessageSize;

      /* Build the SMS device information summary message*/
      MessageEncodeSmsSim(&pMessage, &MessageSize);

      if (MessageSize <= MAX_SMS_LEN)
      {
        /* Ask the modem to send it */
        GsmSmsSend(ConfigGetSmsNumber(), (char*) pMessage);
      }
      else
      {
        TRACE_MODEM("SMS SRM message size %d above MAX_SMS_LEN of %d \n\r", MessageSize, MAX_SMS_LEN);
      }
    }
    else
    {
      TRACE_MODEM("SMS SRM message incorrect version number %c%c\n\r", smsString[0], smsString[1]);
    }
  }
  else
  {
    /* Too short! */
    TRACE_MODEM("SMS SRM message string too short\n\r");
  }
}


/**
 * @brief     Process the incoming SMS Config Value Request Message
 * @param     The SMS string
 * @return    none
 */
static void GsmSmsHandleConfigValueRequestMessage (char *smsString)
{
  char BlockData[80];
  uint8_t BlockType = 1;
  uint8_t BlockParam = 0;  // id of flag or parameter

  TRACE_MODEM("SMS Config Value Request message received\n\r");

  /* Parse message */
  if (strnlen(smsString, MAX_SMS_LENGTH) > 2)
  {
    /* Check version chars */
    if ((smsString[0] == SMS_MSG_VERSION_CHAR_1) && (smsString[1] == SMS_MSG_VERSION_CHAR_2))
    {
      /* get the block type and parameter to read */
      int tmp;
      sscanf(&smsString[2], "%2d", &tmp);
      BlockType = (uint8_t) tmp;
      sscanf(&smsString[4], "%2d", &tmp);
      BlockParam = (uint8_t) tmp;

      /* Encode and Send Status Request message via SMS */
      TRACE_MODEM("Send SMS Config Value Information message, block %d, param %d\n\r", BlockType, BlockParam);

      /* Get the config data */
      if (ConfigTextGetValue(BlockType, BlockParam, BlockData, 80))
      {
        /* Build the SMS Config Value message*/
        uint8_t * pMessage = NULL;
        int BlockLength = strlen(BlockData);
        if (BlockLength < MAX_SMS_LEN)
        {
          int MessageSize;

          MessageEncodeSmsCim(&pMessage, &MessageSize, BlockType, (uint8_t)BlockLength, BlockData);
          if (MessageSize <= MAX_SMS_LEN)
          {
            /* Ask the modem to send it */
            GsmSmsSend(ConfigGetSmsNumber(), (char*) pMessage);
          }
          else
          {
            TRACE_MODEM("SMS message size %d above MAX_SMS_LEN of %d \n\r", MessageSize, MAX_SMS_LEN);
          }
        }
      }
    }
    else
    {
      TRACE_MODEM("SMS message incorrect version number %c%c\n\r", smsString[0], smsString[1]);
    }
  }
  else
  {
    /* Too short! */
    TRACE_MODEM("SMS message string too short\n\r");
  }
}


