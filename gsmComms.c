/**
 **************************************************************************************************
 * @file   gsmComms.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Communication with the GSM modem.
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for GMS Comms */
#define TRACE_GSMCOMMS(...)

/*
#if TRACE_GSMCOMMS_ENABLE
  #define TRACE_GSMCOMMS(...) TRACE_COLOUR_(FG_RED, BG_DEFAULT, "AT", __VA_ARGS__)
#else
  #define TRACE_GSMCOMMS(...)
#endif
*/

/* uncomment to enable hardware flow control on the modem serial port */
//#define GSMCOMMS_USE_HW_FLOW_CONTROL

/* default timeout for AT commands */
#define AT_DEFAULT_TIMEOUT_MS     300

/* ASCII control characters use when decoding AT responses from the modem */
#define CARRIAGE_RETURN           13
#define NEW_LINE                  10
#define CONTROL_Z                 0x1A  /* hex of Control Z */

#define MSG_BUFF_LEN              184
#define SIZE_OF_RESPONSE_BUF      128

/* Message Receipt states */
#define GC_IDLE                   0
#define GC_RECEIVING              1


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void      gsmCommsInit              (void);
static void      gsmCommsProcessData       (char inChar);
static eAtResult gsmCommsWaitForResponse   (int timeoutMs);
static void      gsmCommsProcessBinaryData (uint8_t data);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* UART access */
static stream_t * pStream;

/* Semaphore used to block calling tasks while AT commands are in process */
static SemaphoreHandle_t  AtInProcessMutex;

/* response result, capture from ResponseTable or Timeout of Semaphore */
static eAtResult          AtResult = AT_NONE;

/* response buffer to store current response */
static char responseBuf[SIZE_OF_RESPONSE_BUF];

/*
 * Flag to indicate when we are waiting for response
 * This is a workaround for the fact that binary semaphores are not actually binary
 */
static bool WaitingForResponse = false;

/*
 * When downloading files, we receive binary data (not AT responses) from the modem.  The following
 * variables control the use of binary receive mode
 */
static bool RxBinaryMode = false;
static int  RxBinaryCount = 0;
static int  RxBinaryIndex = 0;
uint8_t     gsmCommsBinaryData[SIZE_OF_RESPONSE_BUF];

/* AT Comms handling */
static int GsmBufIndex = 0;
static int GsmState = GC_IDLE;



/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     The GSM Comms Task function
 * @param     (not used)
 * @return    None
 */
void GsmCommsTask (void const * argument)
{
  uint8_t RxData;

  /* initialise the MCU serial port */
  gsmCommsInit();

  /* create a semaphore used to block calling tasks while AT commands are in process */
  AtInProcessMutex = xSemaphoreCreateBinary();

  /* Main task loop */
  for (;;)
  {
    /*
     * Receive next character from the serial port
     * NOTE: this waits for next character to be received
     */
    WatchdogSetWaiting(GSMCOMMSTASK);
    RxData = pStream->Get();
    WatchdogSetActive(GSMCOMMSTASK);

    if (RxBinaryMode)
    {
      gsmCommsProcessBinaryData(RxData);
    }
    else
    {
      gsmCommsProcessData((char)RxData);
    }
  }
}


/**
 * @brief   Send an AT Command to GSM modem and wait for response
 * @param   pAtCommand - pointer to AT command to send
 * @param   timeout - maximum time to wait for command to complete. If timeout = 0 then
 *                    the function returns immediately without waiting for a response
 * @retval  eResponseCode status indication for the executed command
 */
eAtResult gsmCommsSendAtCommand (char * pAtCommand, int timeoutMs)
{
  TRACE_GSMCOMMS("Tx-%s\n\r", pAtCommand);

  if (isModemAbort())  {

    TRACE_GSMCOMMS("** Modem Aborted, no cmd sent ##**\n\r");
    return (AtResult);
  }
  /* send the command to the modem */
  pStream->Send((uint8_t *)pAtCommand);
  pStream->Put('\r');

  /* wait for the modem to respond (or timeout) */
  AtResult = gsmCommsWaitForResponse(timeoutMs);

  if (AT_TIMEOUT == AtResult)
  {
    TRACE_GSMCOMMS("** Timeout **\n\r");
  }
  return (AtResult);
}



/**
 * @brief   Send a data buffer to the modem
 * @param   pData - pointer to the data to send
 * @param   len - length of the data
 * @param   timeout - maximum time to wait for command to complete. If timeout = 0 then
 *                    the function returns immediately without waiting for a response
 * @retval  eResponseCode status indication for the executed command
 */
eAtResult gsmCommsSendData (uint8_t * pData, int len, int timeoutMs)
{
  uint32_t BytesSent = 0;

  TRACE_GSMCOMMS("Tx-Data...\n\r");

  /*
   * Send the command to the modem.
   * For large packets, we may have to write in several chunks
   */
  while (len)
  {
    BytesSent = pStream->SendBuf(pData, len);
    if (BytesSent != len)
    {
      sleep(5);
    }
    len -= BytesSent;
    pData += BytesSent;
  }

  /* wait for the modem to respond (or timeout) */
  AtResult = gsmCommsWaitForResponse(timeoutMs);

  if (AT_TIMEOUT == AtResult)
  {
    TRACE_GSMCOMMS("** Timeout **\n\r");
  }
  return (AtResult);
}



/**
 * @brief   Sends a data string to the modem and terminates with CTRL-Z
 * @param   msg -  Text msg pointer
 * @retval  none
 */
eAtResult gsmCommsSendString (char *msg, int timeoutMs)
{
  TRACE_GSMCOMMS("Tx-Data...\n\r");

  /* send the command to the modem */
  pStream->Send((uint8_t *) msg);
  pStream->Put(CONTROL_Z);

  /* wait for the modem to respond (or timeout) */
  AtResult = gsmCommsWaitForResponse(timeoutMs);

  if (AT_TIMEOUT == AtResult)
  {
    TRACE_GSMCOMMS("** Timeout **\n\r");
  }
  return (AtResult);
}


/**
 * @brief   Wait for data from the modem
 * @param   timeout - maximum time to wait for the data
 * @retval  eResponseCode status indication for the executed command
 */
eAtResult gsmCommsWaitForData (int timeoutMs)
{
  /* wait for the modem to respond (or timeout) */
  AtResult = gsmCommsWaitForResponse(timeoutMs);

  if (AT_TIMEOUT == AtResult)
  {
    TRACE_GSMCOMMS("** Timeout **\n\r");
  }
  return (AtResult);
}


/**
 * @brief   Initialise serial communications with the modem
 *          Called each time the modem is powered up from sleep
 * @param   none
 * @retval  none
 */
void gsmCommsInitialiseModem (void)
{
  /* reset all module variables */
  AtResult = AT_NONE;
  WaitingForResponse = false;
  RxBinaryMode = false;
  RxBinaryCount = 0;
  RxBinaryIndex = 0;
  GsmBufIndex = 0;
  GsmState = GC_IDLE;

  /* turn echo off */
  gsmCommsSendAtCommand("ATE0", DEFAULT_AT_TIMEOUT_MS);
}


/**
 * @brief   Put the comms layer into binary receive mode.  In this mode:
 *          a) don't echo received data to the console
 *          b) don't scan for ASCII control characters (e.g. \n \r etc)
 * @param   binaryDataLength - number of binary bytes to receive before switching back to AT mode
 * @retval  none
 */
void gsmCommsBinaryMode (int binaryDataLength)
{
  RxBinaryMode  = true;
  RxBinaryCount = binaryDataLength;
  RxBinaryIndex = 0;
}



/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief  Receive binary data from the modem.  This is simply stored as is and no attempt is made
 *         to decode the data (as opposed to AT mode, where the AT responses are decoded)
 * @param  data - the binary data byte received
 * @retval None
 */
static void gsmCommsProcessBinaryData (uint8_t data)
{
  if (RxBinaryCount)
  {
    if (RxBinaryIndex < SIZE_OF_RESPONSE_BUF)
    {
      gsmCommsBinaryData[RxBinaryIndex++] = data;
    }
    RxBinaryCount--;
  }
  if (0 == RxBinaryCount)
  {
    RxBinaryMode = false;
  }
}


/**
  * @brief  Process incoming data from the modem
  *         Sends a message to GSM manager when a complete message has been received
  * @param  inChar
  * @retval None
  */
static void gsmCommsProcessData (char inChar)
{
  /* There are two states, not in message, in message */
  switch (GsmState)
  {
  case GC_IDLE:  /* idle */
    switch (inChar)
    {
    case CARRIAGE_RETURN:
    case NEW_LINE:
      break;
    case '>':
      AtResult = AT_READY_TO_SEND;
      /* command has completed let the caller know */
      xSemaphoreGive(AtInProcessMutex);
      break;
    default:
      GsmBufIndex = 0;
      memset(responseBuf, 0, sizeof(responseBuf));
      responseBuf[GsmBufIndex++] = inChar;
      /* change state */
      GsmState = GC_RECEIVING;
    }
    break;
  case GC_RECEIVING:
    switch (inChar)
    {
    case CARRIAGE_RETURN:
      /* add null terminator */
      if (GsmBufIndex < SIZE_OF_RESPONSE_BUF)
      {
        responseBuf[GsmBufIndex++] = 0;
      }
      break;
    case NEW_LINE:
      TRACE_GSMCOMMS("Rx-%s\n\r", responseBuf);
      /*
       * Send message to modem task for processing
       * The result code indicates whether the command is complete
       */
      AtResult = ModemReceiveATResponse(responseBuf);

      if (WaitingForResponse)
      {
        if ((AT_COMPLETE == AtResult) || (AT_ERROR == AtResult))
        {
          /* command has completed so let the caller know */
          xSemaphoreGive(AtInProcessMutex);
        }
      }
      /* change state */
      GsmState = GC_IDLE;
      break;
    default:
      if (GsmBufIndex < SIZE_OF_RESPONSE_BUF)
      {
        responseBuf[GsmBufIndex++] = inChar;
      }
    }
    break;
  }
}

/**
 * @brief  Wait for command to complete or timeout by taking blocking semaphore
 * @param  timeoutMs
 * @retval return AtResult from Modem or Timeout
 */
eAtResult gsmCommsIssueWaitForResponse (int waitTimeMs)
{
  WaitingForResponse = true;

  /*
   * Wait for command to complete or timeout
   * WARNING - this blocks the calling task
   */
  if (xSemaphoreTake(AtInProcessMutex, waitTimeMs/portTICK_PERIOD_MS ) != pdTRUE )
  {
    AtResult = AT_TIMEOUT;
  }

  /* When come here, AtInProcessMutex Unblocked,
   * either Modem response or timeout
   */
  WaitingForResponse = false;

  return AtResult;
}


/**
 * @brief  Wait for the modem to respond, or timeout after the specified period
 * @param  timeoutMs
 * @retval return AtResult from Modem or Timeout
 */
eAtResult gsmCommsWaitForResponse (int timeoutMs)
{
  uint16_t slice = timeoutMs/DEFAULT_AT_TIMEOUT_MS;
  uint16_t remainsMs = timeoutMs % DEFAULT_AT_TIMEOUT_MS;

  for (uint16_t i = 0; i < slice; i++)
  {
    AtResult = gsmCommsIssueWaitForResponse(DEFAULT_AT_TIMEOUT_MS);

    /* response got or Modem Action Abort, exit immediately*/
    if ((AT_TIMEOUT != AtResult) || (isModemAbort()))
      break;

  }

  if ((remainsMs) && (AT_TIMEOUT == AtResult) && !isModemAbort())
  {
    /* still have a few milliseconds to wait */
    AtResult = gsmCommsIssueWaitForResponse(remainsMs);
  }

  return AtResult;
}


/**
 * @brief  Initialise the UART used for GSM communication
 * @param  None
 * @retval None
 */
static void gsmCommsInit (void)
{
  uartconfig_t UartConfig;

  UartConfig.BaudRate = 115200;
  UartConfig.DataBits = COM_DATABITS_8;
  UartConfig.StopBits = COM_STOPBITS_1;
  UartConfig.Parity   = COM_PARITY_NONE;
#ifdef GSMCOMMS_USE_HW_FLOW_CONTROL
  UartConfig.FlowControl = COM_FLOWCONTROL_HW;
#else
  UartConfig.FlowControl = COM_FLOWCONTROL_NONE;
#endif
  pStream = UARTInitialise(GSM_UART, &UartConfig);
}

