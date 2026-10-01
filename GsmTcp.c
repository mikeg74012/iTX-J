/**
 **************************************************************************************************
 * @file   gsmTcp.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements raw TCP functionality for GSM Modems.
 *
 * @notes: Only one TCP context is currently provided.  This module could be modified to provide
 *         multiple contexts in the future
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
// Enable of disable verbose printing of message data
#define GSMTCP_PRINT_MESSAGES        1

// Timeout defines
#define TCP_CONNECT_TIMEOUT_MS       120000
#define TCP_DATA_TIMEOUT_MS          60000
#define TCP_RXDATA_DELAY_S           15

// Maximum download size from cloud to the GSM modem (in bytes).  Limited by modem RAM size
#define MAX_DOWNLOAD_SIZE            (400 * 1024)

// Default file transfer size from modem to SPI SRAM (in bytes)
#define DEFAULT_TRANSFER_SIZE        128




/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern int     ModemFileHandle;
extern int     ModemFileSize;
extern uint8_t gsmCommsBinaryData[];
extern int     GsmTcpUnacknowledgedCount;
extern bool    TcpDataNotification;
extern bool    TcpStored;
extern char    textTcpReceived[MAX_SMS_LEN];
extern bool    ServerConnected;        /* flag indicates when a TCP server is connected */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static char AtCommand[MAX_CONSOLE_AT_LENGTH];


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool GsmTcpSetupSSL            (bool useSSL);
static bool GsmTcpSetupDataFormats    (void);
static bool GsmTcpConnect             (char * ipAddress, uint32_t port);
static bool GsmTcpSendData            (uint8_t * pData, int dataLen);
static void GsmTcpCheckAndReceive     (void);
static bool GsmTcpGetReceivedData     (void);
static bool GsmTcpProcessReceivedData (void);

#if GSMTCP_PRINT_MESSAGES
  static void GsmTcpPrintMessage       (uint8_t * pBuffer, int Length);
#endif


/*
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS (API)
 **************************************************************************************************
 */

/**
 * @brief       Configure the TCP connection, send the data and read the response if present
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   size     : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
bool GsmTcpConfigAndSend (uint8_t * pData, uint32_t size)
{
  bool useSSL = false;
  bool result = true;

  /* Configure TCP security */
  result = GsmTcpSetupSSL(useSSL);

  /* Configure receive data format */
  if (result)
  {
    result = GsmTcpSetupDataFormats();
  }

  /* Connect to the remote TCP server */
  if (result)
  {
    sleep(1000);
    result = GsmTcpConnect(ConfigGetIpAddress(), ConfigGetIpPort());
  }

  /* POST the message */
  if ((result) && (ServerConnected))
  {
    result = GsmTcpSendData(pData, size);
  }

  /* Process any incoming data */
  GsmTcpCheckAndReceive();

  return result;
}


/**
 * @brief       Configure the TCP(S) connection
 * @param       none
 * @return      bool     : true on success, false on failure
 */
bool GsmTcpConfigure (void)
{
  bool useSSL = false;
  bool result = true;

  /* Configure TCP security */
  result = GsmTcpSetupSSL(useSSL);

  /* Configure receive data format */
  if (result)
  {
    result = GsmTcpSetupDataFormats();
  }

  /* Connect to the remote TCP server */
  if (result)
  {
    sleep(1000);
    result = GsmTcpConnect(ConfigGetIpAddress(), ConfigGetIpPort());
  }
  return result;
}


/**
 * @brief       POST the data and READ the response
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   size     : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
bool GsmTcpSend (uint8_t * pData, uint32_t size)
{
  /* POST the message */
  bool result = GsmTcpSendData(pData, size);

  /* Process any incoming data */
  GsmTcpCheckAndReceive();

  /* READ the response from the server */
  if (result)
  {
#if GSMTCP_PRINT_MESSAGES
    GsmTcpPrintMessage(pData, size);
#endif
  }

  return result;
}

/**
 * @brief     Modem will stay in data mode, if the GsmTcpUnacknowledgedCount is not zero.
 *            flush it to make it back to AT command mode
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
bool GsmTcpFlush (void)
{
  eAtResult AtResult = AT_COMPLETE;

  if (GsmTcpUnacknowledgedCount)
  {
    /* Flush the Modem with the space char */
    snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "%*s", GsmTcpUnacknowledgedCount, " ");
    AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Close all TCP connection
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
bool GsmTcpClose (void)
{
  eAtResult AtResult;

  /* send the AT command to get the data from the modem */
  AtResult = gsmCommsSendAtCommand("AT+QICLOSE", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}



/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief       Configure AT data formats
 * @param[in]   none
 * @return      bool     : true on success, false on failure
 */
static bool GsmTcpSetupDataFormats (void)
{
  eAtResult AtResult = AT_COMPLETE;

  /* Configure modem to notify us when tcp data is received */
  AtResult = gsmCommsSendAtCommand("AT+QINDI=1", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}



/**
 * @brief       Configure the TCP connection type
 * @param[in]   useSSL   : if true, the connection is configured to use TCPS / SSL
 * @return      bool     : true on success, false on failure
 */
static bool GsmTcpSetupSSL (bool useSSL)
{
  eAtResult AtResult = AT_COMPLETE;

#if TCP_USE_SSL
  /*
   * Disable SSL
   * We cannot make any security changes if SSL enabled, and it is simpler and quicker to always
   * disable it than to check if it is already enabled
   */
  AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"https\",0", DEFAULT_AT_TIMEOUT_MS);

  /* enable SSL if required */
  if (useSSL)
  {
    if (AT_COMPLETE == AtResult)
    {
      /* Supported SSL levels - ALL (TLS1.0, TLS1.1, TLS1.2, SSL3.0) */
      AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"sslversion\",0,4", DEFAULT_AT_TIMEOUT_MS);
    }

    if (AT_COMPLETE == AtResult)
    {
      /* Security level 0 - No authentication required */
      AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"seclevel\",0,0", DEFAULT_AT_TIMEOUT_MS);
    }

    /* enable SSL */
    if (AT_COMPLETE == AtResult)
    {
      AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"https\",1", DEFAULT_AT_TIMEOUT_MS);
    }
    /* set the SSL Context */
    if (AT_COMPLETE == AtResult)
    {
      AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"httpsctxi\",0", DEFAULT_AT_TIMEOUT_MS);
    }
  }
  else
  {
    if (AT_COMPLETE == AtResult)
    {
      AtResult = gsmCommsSendAtCommand("at+qsslcfg=\"https\",0", DEFAULT_AT_TIMEOUT_MS);
    }
  }

#endif
  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief       Connect to remote TCP server
 * @param[in]   ipAddress : IP address of the remote server
 * @param[in]   port      : the IP port number of the remote server
 * @return      bool      : true on success, false on failure
 */
static bool GsmTcpConnect (char * ipAddress, uint32_t port)
{
  eAtResult AtResult;

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QIOPEN=\"TCP\",\"%s\",\"%d\"", ipAddress, (int)port);

  /* send the AT commands to set the URL */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  /* we must wait for "CONNECT OK", which comes AFTER the first "OK" */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsWaitForData(TCP_CONNECT_TIMEOUT_MS);
  }

  if (AT_COMPLETE != AtResult)
  {
    /* Log down the network error */
    ModemLogNetworkError(NETWORK_TCP_ERROR);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief       Send data to the remote TCP server
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   dataLen  : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
static bool GsmTcpSendData (uint8_t * pData, int dataLen)
{
  eAtResult AtResult;

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QISEND=%d", dataLen);

  /* send the AT command to set the data length */
  AtResult = gsmCommsSendAtCommand(AtCommand, TCP_CONNECT_TIMEOUT_MS);

  /* wait for the data to send.  NOTE: There may be a better way to do this */
  int RetryCount = 0;
  GsmTcpUnacknowledgedCount = dataLen;

  /* now send the data itself */
  if (AT_READY_TO_SEND == AtResult)
  {
    AtResult = gsmCommsSendData(pData, dataLen, TCP_DATA_TIMEOUT_MS);
  }

  while ((AT_COMPLETE == AtResult) && (GsmTcpUnacknowledgedCount) && (RetryCount < 30))
  {
    if (isModemAbort())
      break;

    sleep(1000);
    AtResult = gsmCommsSendAtCommand("AT+QISACK", DEFAULT_AT_TIMEOUT_MS);
    RetryCount++;
  }

  /* Some Packets cant send out completely (Not from Abort) */
  if ((GsmTcpUnacknowledgedCount) && (AtResult != AT_COMPLETE))
  {
    ModemLogNetworkError(NETWORK_PACKETSEND_ERROR);
  }

  return (((AtResult == AT_COMPLETE) ? true : false) && (GsmTcpUnacknowledgedCount == 0));
}


/**
  * @brief    Check for received TCP data and read it if present
  *
  *           NOTE: Curently we check for received data after every packet we send.  However, in some cases
  *                 the response is not received for a long time > 20 seconds.  This is inefficient and
  *                 could lead to some responses being missed.  If this proves to be a problem, we should
  *                 change the mechanism to work more like the SMS Receive (see SeqHearbeat.c).  This would
  *                 Need to be separate from heartbeat though because that does not account for journey messages.
  *                 We would also need to make sure we do not do the GPRSDisconnect() after sending data, but
  *                 instead wait until the whole sequence is complete.
  * @param    None
  * @retval   None
  */
static void GsmTcpCheckAndReceive (void)
{
  bool Result = false;

  /* check for data for fixed period before giving up */
  for (int i = 0; i < TCP_RXDATA_DELAY_S; i++)
  {
    if (isModemAbort()) break;

    sleep(1000);
    if (true == TcpDataNotification) break;
  }

  if (true == TcpDataNotification)
  {
    TcpDataNotification = false;
    Result = GsmTcpGetReceivedData();

    if ((Result) && (TcpStored))
    {
      TcpStored = false;
      Result = GsmTcpProcessReceivedData();
    }
  }
}


/**
  * @brief    Get the received TCP data from the modem.  Should be called after a
  *           the modem issues a received data notifiation
  * @param    None
  * @retval   true on success, false on failure
  */
static bool GsmTcpGetReceivedData (void)
{
  eAtResult AtResult;

  /* send the AT command to get the data from the modem */
  AtResult = gsmCommsSendAtCommand("AT+QIRD=0,1,0,1024", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
  * @brief    Process incoming TCP data and forward to the correct module for processing
  * @param    None
  * @retval   true on success, false on failure
  */
static bool GsmTcpProcessReceivedData (void)
{
  bool parsedOK = false;

  TRACE_MODEM("Received TCP: %s\n\r", textTcpReceived);

  /* Parse TCP data */
  uint16_t stringLength = strnlen(textTcpReceived, MAX_SMS_LENGTH);

  if ((int16_t)(stringLength-SMS_CRC_LENGTH) > 0)
  {
    /* Check the CRC */
    unsigned int readCRC;
    if (sscanf(&textTcpReceived[stringLength-SMS_CRC_LENGTH], "%4X", &readCRC) == 1)
    {
      uint16_t calcCRC = CrcDriverCalculateCrc16((uint8_t *)textTcpReceived, stringLength-SMS_CRC_LENGTH);

      /* Calculate CRC */
      TRACE_MODEM("TCP Read CRC = %4X, TCP Expected CRC = %4X\n\r", readCRC, calcCRC);

      if ((uint16_t) readCRC == calcCRC)
      {
        parsedOK = true;

        if (textTcpReceived[SMS_MESSAGE_TYPE_IDX] == SMS_MESSAGE_TYPE_CONFIG)
        {
          /* set config values */
          (void)ConfigHandleViaSMS(&textTcpReceived[SMS_MESSAGE_TYPE_IDX+1]);
        }
        else if (textTcpReceived[SMS_MESSAGE_TYPE_IDX] == SMS_MESSAGE_TYPE_CONFIG_REQ)
        {
          /* get config values */
          MessageEnqueueCim();
        }
        else
        {
          TRACE_MODEM("Error: Incoming TCP type unknown!!!\n\r");
        }
      }
      else
      {
        TRACE_MODEM("Error: Received TCP CRC failed!!\n\r");
      }
    }
    else
    {
      TRACE_MODEM("Error reading TCP CRC field\n\r");
    }
  }
  else
  {
    TRACE_MODEM("TCP CRC Length error (msg length = %d)\n\r", (int16_t)(stringLength-SMS_CRC_LENGTH));
  }
  return parsedOK;
}



#if GSMTCP_PRINT_MESSAGES
/**
 * @brief  Print the message data
 * @param  pBuffer : pointer to message data
 * @param  Length  : the number of bytes to print
 * @retval None
 */
static void GsmTcpPrintMessage (uint8_t * pBuffer, int Length)
{
  /* print the header */
  ConsolePrintf("---------- MESSAGE SENT VIA TCP ----------\n\r");

  for (int i = 0; i < Length; i++)
  {
//    ConsolePrintf("%02x,", pBuffer[i]);
    ConsolePrintf("%d,", pBuffer[i]);
    if (0 == (i+1) % 16)
    {
//      ConsolePrintf("\n\r");
      sleep(10);
    }
  }
  ConsolePrintf("\n\r");
}
#endif



/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

 /**
 * @brief       Set Config data as if incoming TCP message string
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmConfigConsoleTCP (int argCount, char **pArguments)
{
  /**
   * Tested with follow by the SMS Builder 
   * 
   * Customer ID
   * Customer: 67 "C01090026700CC87"
   * 
   * APN related
   * URL: stream.co.uk Username:streamip password:streamip  "C0112012stream.co.uk13008streamip14008streamip00AA2B"
   * URL: abc.xyz.pqr.com Username:u!"$%^'#@~ password:p&*()_+,./<>?  "C0112015abc.xyz.pqr.com13010u!"$%^'#@~14013p&*()_+,./<>?00B9B3"
   * 
   * SMS and Voice Number
   * set with value +447483226805 "C0101013+44748322680502013+447483226805005B07"
   * 
   * Flag and Parameter related
   * Flag setting Flag 0-4 with 1,   "C01080030010800301108003021080030310800304100E68E"
   * Flag setting Flag 5-9 with 1,   "C0108003051080030610800307108003081080030910051E9"
   * Flag setting Flag 10-14 with 1, "C01080031010800311108003121080031310800314100D8E7"
   * Flag setting Flag 15 with 1,    "C010800315100BA9C"
   * 
   * Param 0-15 with value 44
   * Param 0, "C011000400440021F6"
   * Param 1, "C01100040144008BA7"
   * Param 2, "C01100040244006575"  
   * Param 3, "C0110004034400CF24"
   * Param 4, "C0110004044400A8F0"
   * 
   * Param 1,2,3,4, with value 44 "C0110004014410004024410004034410004044400F13A"
   * Param 5,6,7,8, with value 44 "C01100040544100040644100040744100040844007680"
   * Param 9,10,11,12 with value 120000 "C01100080912000010008101200001000811120000100081212000000F141"
   * Param 13,14,15   with value 120000 "C01100081312000010008141200001000815120000004F64"
   * 
   * IP Address and port
   * 192.168.1.100, port 1234 "C0104013192.168.1.100050041234001FDA"
   * 
   * gprsurl 
   * https://itx.azure.net/api/Listener (shorten becoz of the limit of console buffer) "C0103034https://itx.azure.net/api/Listener00EC7B"
   * 
   * FwURL 
   * https://syno.ripcorddesigns.com/client/itelematix/beta "C0106054https://syno.ripcorddesigns.com/client/itelematix/beta00F256"
   * 
   */
    
  if (1 == argCount)
  {
    strncpy(textTcpReceived, pArguments[0], MAX_SMS_LEN);
    textTcpReceived[MAX_SMS_LEN-1] = '\0';
    GsmTcpProcessReceivedData();
    return (CONSOLE_ERROR_OK);
  }
  return(CONSOLE_ARG_NO);
}
