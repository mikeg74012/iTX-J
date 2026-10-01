/**
 **************************************************************************************************
 * @file   gsmHttp.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements HTTP functionality for GSM Modems.
 *
 * @notes: Only one HTTP context is currently provided.  This module could be modified to provide
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
#define GSMHTTP_PRINT_MESSAGES       0

// Timeout defines
#define HTTP_CONNECT_TIMEOUT_MS      120000
#define HTTP_DATA_TIMEOUT_MS         120000

// Maximum download size from cloud to the GSM modem (in bytes).  Limited by modem RAM size
#define MAX_DOWNLOAD_SIZE            (400 * 1024)

// Default file transfer size from modem to SPI SRAM (in bytes)
#define DEFAULT_TRANSFER_SIZE        128


/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern int      ModemFileHandle;
extern int      ModemFileSize;
extern uint8_t  gsmCommsBinaryData[];
extern bool     ServerConnected;        /* flag indicates when a TCP server is connected */


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
static bool GsmHttpSetupSSL       (bool useSSL);
static bool GsmHttpSetUrl         (char * pUrl);
static bool GsmHttpPost           (uint8_t * pData, int dataLen);
static bool GsmHttpRead           (void);
static bool GsmHttpGet            (void);
static bool GsmHttpDownload       (uint32_t maxSize);
static bool GsmHttpMoveFileToSram (uint32_t sramAddress);
#if GSMHTTP_PRINT_MESSAGES
static void GsmHttpPrintMessage (uint8_t * pBuffer, int Length);
#endif


/*
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS (API)
 **************************************************************************************************
 */

/**
 * @brief       Configure the HTTP(S) connection, POST the data and READ the response
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   size     : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
bool GsmHttpConfigAndSend (uint8_t * pData, uint32_t size)
{
  bool useSSL = true;

  /* Configure HTTP security */
  bool result = GsmHttpSetupSSL(useSSL);

  /* Set the URL */
  if (result)
  {
    result = GsmHttpSetUrl(ConfigGetGprsUrl());
  }

  /* POST the message */
  if (result)
  {
    result = GsmHttpPost(pData, size);
  }

  /* READ the response from the server */
  if (result)
  {
    result = GsmHttpRead();
  }

  return result;
}


/**
 * @brief       Configure the HTTP(S) connection
 * @param       none
 * @return      bool     : true on success, false on failure
 */
bool GsmHttpConfigure (void)
{
  bool useSSL = true;

  /* Configure HTTP security */
  bool result = GsmHttpSetupSSL(useSSL);

  /* Set the URL */
  if (result)
  {
    result = GsmHttpSetUrl(ConfigGetGprsUrl());
  }
  return result;
}


/**
 * @brief       POST the data and READ the response
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   size     : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
bool GsmHttpSend (uint8_t * pData, uint32_t size)
{
  /* POST the message */
  bool result = GsmHttpPost(pData, size);

  /* READ the response from the server */
  if (result)
  {
#if GSMHTTP_PRINT_MESSAGES
    GsmHttpPrintMessage(pData, size);
#endif
    result = GsmHttpRead();
  }

  return result;
}


/**
 * @brief       Download a file into the modems internal RAM file system
 * @param[in]   size     : expected file size, in bytes
 * @return      bool     : true on success, false on failure
 */
bool GsmHttpDownloadFile (char * url, uint32_t maxSize, uint32_t sramAddress)
{
  bool useSSL = true;

  /* Configure HTTP security */
  bool result = GsmHttpSetupSSL(useSSL);

  /* Set the URL */
  if (result)
  {
    result = GsmHttpSetUrl(url);
  }

  /* Send the HTTP "Get" to request the resource */
  if (result)
  {
    result = GsmHttpGet();
  }

  /* Download the file from the server */
  if (result)
  {
    result = GsmHttpDownload(maxSize);
  }

  /* copy the file from the modem into SPI SRAM */
  if (result)
  {
    GsmHttpMoveFileToSram(sramAddress);
  }

  return result;
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief       Configure the HTTP connection type
 * @param[in]   useSSL   : if true, the connection is configured to use HTTPS / SSL
 * @return      bool     : true on success, false on failure
 */
static bool GsmHttpSetupSSL (bool useSSL)
{
  eAtResult AtResult;

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

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief       Configure the modem to use the requested URL
 * @param[in]   pUrl     : pointer to the URL to use
 * @return      bool     : true on success, false on failure
 */
static bool GsmHttpSetUrl (char * pUrl)
{
  eAtResult AtResult;

  /* get the length of the URL */
  int UrlLen = strlen(pUrl);

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qhttpurl=%d,30", UrlLen);

  /* send the AT commands to set the URL */
  AtResult = gsmCommsSendAtCommand(AtCommand, HTTP_CONNECT_TIMEOUT_MS);
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendAtCommand(pUrl, HTTP_DATA_TIMEOUT_MS);
  }
  sleep(500);

  return ((AtResult == AT_COMPLETE) ? true : false);
}



/**
 * @brief       POST data to the HTTP(S) server
 * @param[in]   pData    : pointer to the data to send
 * @param[in]   dataLen  : length of the data, in bytes
 * @return      bool     : true on success, false on failure
 */
static bool GsmHttpPost (uint8_t * pData, int dataLen)
{
  eAtResult AtResult;

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qhttppost=%d,30,30", dataLen);

  /* send the AT commands to set the Data */
  AtResult = gsmCommsSendAtCommand(AtCommand, HTTP_CONNECT_TIMEOUT_MS);
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendData(pData, dataLen, HTTP_DATA_TIMEOUT_MS);
  }

  if (AT_COMPLETE != AtResult)
  {
    /* Log down the network error */
    ModemLogNetworkError(NETWORK_HTTP_ERROR);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     READ the server response to our previous operation
 *            NOTE: The modem responds with 3 separate messages, as follows:
 *                  1. "CONNECT"
 *                  2. The data
 *                  3. "OK"
 * @param[in] none
 * @return    bool     : true on success, false on failure
 */
static bool GsmHttpRead (void)
{
  /*
   * Request the response from the server
   * NOTE: The modem response is as follows:
   *       1. "CONNECT"
   *       2. The data
   *       3. "OK"
   */
  eAtResult AtResult = gsmCommsSendAtCommand("at+qhttpread=60", HTTP_CONNECT_TIMEOUT_MS);

  /* now wait for the data and "OK" */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsWaitForData(HTTP_DATA_TIMEOUT_MS);
  }

  if (AT_COMPLETE != AtResult)
  {
    /* Log down the network error */
    ModemLogNetworkError(NETWORK_HTTP_ERROR);
  }
  
  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief       Send GET request to the server.  Must be followed by READ to actually read the data
 * @param[in]   none
 * @return      bool     : true on success, false on failure
 */
static bool GsmHttpGet (void)
{
  eAtResult AtResult = gsmCommsSendAtCommand("at+qhttpget=30", HTTP_CONNECT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Download a file into the modem internal RAM filesystem
 * @param[in] maxSize  : maximum file size to download
 * @return    bool     : true on success, false on failure
 */
static bool GsmHttpDownload (uint32_t maxSize)
{
  eAtResult AtResult = AT_ERROR;

  /* Clip the download size */
  if (maxSize > MAX_DOWNLOAD_SIZE)
  {
    maxSize = MAX_DOWNLOAD_SIZE;
  }

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qhttpdl=\"RAM:download.rip\",%d", (int)maxSize);

  /* Download the file */
  AtResult = gsmCommsSendAtCommand(AtCommand, HTTP_CONNECT_TIMEOUT_MS);

  /* wait for the download to complete, and log the file size
   * NOTE: the download command returns "OK" first, then returns "+QHTTPDL" when the download is complete */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsWaitForData(HTTP_DATA_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}




/**
 * @brief     Move a file from the modem file system to SPI SRAM
 * @param[in] sramAddress : Address in SPI SRAM to store the file
 * @return    bool        : true on success, false on failure
 */
static bool GsmHttpMoveFileToSram (uint32_t sramAddress)
{
  eAtResult AtResult;
  int Offset = 0;
  int TransferLength = 128;

  /* get the file size in bytes */
  AtResult = gsmCommsSendAtCommand("at+qflst=\"RAM:download.rip\"", DEFAULT_AT_TIMEOUT_MS);

  /* open the file and get the file handle*/
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendAtCommand("at+qfopen=\"RAM:download.rip\"", DEFAULT_AT_TIMEOUT_MS);
  }

  if (AT_COMPLETE == AtResult)
  {
    /*
     * File is OPEN
     * read the data out in blocks, write each block to SRAM
     */
    while (ModemFileSize > 0)
    {
      if (isModemAbort())
      {
        /* quit immediate if someone request the modem action abort */
        break;
      }

      /* calculate the transfer length */
      if (ModemFileSize > DEFAULT_TRANSFER_SIZE)
      {
        TransferLength = DEFAULT_TRANSFER_SIZE;
      }
      else
      {
        TransferLength = ModemFileSize;
      }

      /* seek to the next file offset */
      if (AT_COMPLETE == AtResult)
      {
        snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qfseek=%d,%d", ModemFileHandle, Offset);
        AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
      }

      /*
       * Read the next block of data
       * NOTE The modem will respond with CONNECT <data len>, and then the binary data.
       *      We use this CONNECT message to put the AT Comms layer into a special BINARY data state which stores <data len> bytes
       *      into the data buffer without looking for the normal ASCII control characters
       */
      if (AT_COMPLETE == AtResult)
      {
        snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qfread=%d,%d", ModemFileHandle, TransferLength);
        AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
      }

      /* Store the block of data to SRAM */
      if (AT_COMPLETE == AtResult)
      {
        if (false == SerialRamWrite((sramAddress + Offset), &gsmCommsBinaryData[0], TransferLength))
        {
          TRACE_MODEM("Failed to write to SPI SRAM\n\r");
          break;
        }
      }

      /* update the pointers */
      Offset = Offset + TransferLength;
      ModemFileSize = ModemFileSize - TransferLength;
    }

    /* Always close the file if we opened it, failing to do so will prevent deleting it */
    snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qfclose=%d", ModemFileHandle);
    AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
  }

  /* Always delete the file, regardless of whether we managed to open it */
  gsmCommsSendAtCommand("at+qfdel=\"RAM:*\"", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


#if GSMHTTP_PRINT_MESSAGES
/**
 * @brief  Print the message data
 * @param  pBuffer : pointer to message data
 * @param  Length  : the number of bytes to print
 * @retval None
 */
static void GsmHttpPrintMessage (uint8_t * pBuffer, int Length)
{
  int i, j = 0;

  /* print the header */
  ConsolePrintf("---------- MESSAGE SENT VIA HTTP ----------\n\r");
  for (i = 0; i <= (Length / 16); i++)
  {
    for (j = 0; j < 16; j++)
    {
      ConsolePrintf(" %02x", pBuffer[(i*16) + j]);
    }
    ConsolePrintf("\n\r");
  }
}
#endif


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Configure the HTTP(S) connection from the console
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleConfigure (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* for now the configuration is fixed */
    GsmHttpSetupSSL(true);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Set the URL to use for HTTP operations.  If the argument is blank,
 *              the default URL from the config store is used
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleSetUrl (int argCount, char **pArguments)
{
  switch (argCount)
  {
    case 0:
      /* use the URL from the config store */
      GsmHttpSetUrl(ConfigGetGprsUrl());
      break;
    case 1:
      /* argument contains the server URL */
      GsmHttpSetUrl(pArguments[0]);
      break;
    default:
      return (CONSOLE_ARG_NO);
  }
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       POST a message to the connected server.  URL must be set first
 *              using GsmHttpSetURL().
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsolePost (int argCount, char **pArguments)
{
  char Message[] = "Ripcord Test Message";

  switch (argCount)
  {
    case 0:
      /* POST a dummy message */
      GsmHttpPost( (uint8_t *)Message, sizeof(Message) );
      break;
    case 1:
      /* argument contains the message to POST */
      GsmHttpPost((uint8_t *)pArguments[0], strlen(pArguments[0]));
      break;
    default:
      return (CONSOLE_ARG_NO);
  }
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Read the server response to the previous POST
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleRead (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* Read the response from the server */
    GsmHttpRead();

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       GET a resource from the connected server.  URL must be set first
 *              using GsmHttpSetURL()
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleGet (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* Request a resource from the server */
    GsmHttpGet();

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Test TCP by send a msg to URL and check the ACK
 *               calling ModemHandleSendTCPQueued ();
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleTest (int argCount, char **pArguments)
{
  ServerConnected = false;
  if (0 == argCount)
  {
    /*send the msg and wait response from the server and update the flag, the flag can be checked later by the test APP */
    ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       List the ServerConnected flag status, for factory test purpose
 *
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmHttpConsoleFlag (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    if (ServerConnected)
    {
      ConsolePrintf ("server connection: --ok-- \n\r");
    }
    else
    {
      ConsolePrintf ("server connection: unknown\n\r");
    }
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


