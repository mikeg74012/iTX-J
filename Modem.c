/*
 **************************************************************************************************
 * @file   modem.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-14
 *
 * @brief  Quectel MC60E modem driver, main thread
 *
 **************************************************************************************************
 * @attention                     (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define GNSS_GETDATA_TIMEOUT_SECS      10
#define GNSS_GETVALIDDATA_TIMEOUT_SECS 20

/* length of message queue */
#define MODEM_QUEUE_LENGTH             16

/* time for modem to boot after its power is turned on */
#define MODEM_BOOT_TIME_MS             3000

/* ASCII code for '0' */
#define ASCII_ZERO                     48

/* Feature enable flags (1=enable, 0=disable) */
#define MODEM_LOG_RSSI                 1
#define MODEM_LOG_MODULE_VERSION       1
#define MODEM_JAMMING_DETECTION        1
#define MODEM_COUNTRY_CODE_CHECK       1

/* gsm power control mode */
#define POWER_UP_IN_SEQUENCE           0
#define POWER_UP_IN_JOURNEY            1
#define GSM_POWER_ON_DELAY             1100
#define GSM_POWER_OFF_DELAY            800

/*
 * When the modem sends data from the message queues we can decide whether to send
 * data from only the selected queue, or always send data from BOTH queues
 * Note: Comment out the define to send from selected queue only
 */
#define MODEM_ALWAYS_SEND_BOTH_QUEUES

/* Structure to define how incoming messages from the modem are handled */
typedef struct
{
  char *    ResponseString;
  bool *    Flag;
  bool      FlagValue;
  void      (*pHandlerFunc)  (char * msgString);
  eAtResult AtResultCode;
} ResponseHandler_t;


/* Structure for passing data buffers by reference */
typedef struct
{
  uint8_t * pData;  // pointer to the data buffer
  uint32_t  Size;   // size of the data
} ModemData_t;


/* Structure for describing an HTTP file download */
typedef struct
{
  char *    Url;         // url of the file to download
  uint32_t  MaxSize;     // maximum size to download and store (bytes)
  uint32_t  SramAddress; // address in the spi sram to store the file
} ModemDownload_t;


/* Modem power supply states */
typedef enum
{
  MPS_ALL_OFF,
  MPS_ALL_ON,
  MPS_GSM_OFF_GNSS_ON,
  MPS_GSM_ON_GNSS_OFF
} eModemPowerState;


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static void      AtProcessImeiData         (char * msgString);
static void      ModemEnqueueMessage       (uint8_t msgId, void * parameter);
static void      ModemGpioInit             (void);
static void      ModemGpioDeInit           (void);

/* Functions used by the modem task to perform each high level function requested by the sequencer */
static void      ModemProcessMessage       (ModemMessage_t message);
static bool      ModemHandlePowerUp        (void);
static bool      ModemHandlePowerDown      (void);
static bool      ModemHandlePowerDownGsm   (void);
static bool      ModemHandleCheckGSMInfo   (void);
static bool      ModemHandleMakeVoiceCall  (char * pNumber);
static bool      ModemHandleSendTCP        (uint8_t * pData, uint32_t size);
static bool      ModemHandleSendTCPQueued  (MessagePriority_e priority);
static bool      ModemSpoolFromQueueTCP    (MessagePriority_e priority);
static bool      ModemHandleSendHTTP       (uint8_t * pData, uint32_t size);
static bool      ModemHandleSendHTTPQueued (MessagePriority_e priority);
static bool      ModemSpoolFromQueueHTTP   (MessagePriority_e priority);
static bool      ModemHandleHTTPDownload   (ModemDownload_t * pModemDownload);
static bool      ModemHandleGnssStart      (void);
static bool      ModemHandleGnssStop       (void);
static bool      ModemHandleJamControl     (bool bMode);
static bool      ModemHandleGnssGetData    (bool storeToSram);
static bool      ModemHandleAudioConfigure (void);
static bool      ModemHandleAudioPlayTone  (eModemTone toneType, uint8_t mode);
static bool      ModemHandleAudioStopTone  (void);
static bool      ModemHandleSmsSend        (char * pMessage);
static bool      ModemHandleSmsReceive     (void);
static bool      ModemHandleBleScan        (void);
static bool      ModemHandleModemAbort     (void);
static bool      ModemHandleToneUpload     (eModemTone modemTone);
static bool      ModemPowerDownAllowed     (void);
static bool      ModemPWRkeyControl        (bool powerUp);
static void      ModemGSMPowerControl      (bool PowerEnable);
static bool      ModemPowerOn              (void);
static void      ModemUpdateIMEI           (void);

/* functions used to decode and process AT messages from the modem */
static eAtResult ModemDecodeATResponse     (char * pAtResult);
static void      ModemDecodeGnssMessage    (char * pMessage);
static void      AtProcessSMSRxData        (char * pMessage);
static void      AtProcessPositionInfo     (char * pMessage);
static void      AtProcessVelocityInfo     (char * pMessage);
static void      AtProcessGGAInfo          (char * pMessage);
static void      AtProcessNetworkCellID    (char * pMessage);
static void      AtProcessNetworkRSSI      (char * pMessage);
static void      AtProcessFileOpen         (char * pMessage);
static void      AtProcessFileList         (char * pMessage);
static void      AtProcessDownloadConnect  (char * pMessage);
static void      AtProcessTcpAck           (char * pMessage);
static void      AtProcessBleScan          (char * pMessage);
static void      AtProcessTcpRxData        (char * pMessage);
static void      AtProcessGSMNetworkInfo   (char * pMessage);


/*
 **************************************************************************************************
 *                                        GLOBAL VARIABLES
 **************************************************************************************************
 */
ModemStatus_t ModemStatus;

/* Modem status Flags */
bool ModemReady          = false;
bool RegisteredOnNetwork = false;   // MOVE THE USEFUL STUFF INTO ModemStatus structure
bool RegisteredOnGPRS    = false;
bool GSMOperatorInfoGot  = false;   /* flag to tell if the operator Info got */
bool callConnected       = false;   /* flag when call is connected */  /* used in gsmVoice.c */
bool ServerConnected     = false;   /* flag set when server is connected */
bool apnSet              = false;   /* to check if apn already set */
bool smsStored           = false;   /* incoming sms stored in buffer */
bool GNSSDataStored      = false;
bool EPOEnabled          = false;
bool GNSSPowerON         = false;
bool velocityStored      = false;
bool gnssDataValid       = false;
bool GNSSTimeSync        = false;
bool NTPTimeSync         = false;   /* flag that indicates if we have got NTP time sync */
int  ModemFileHandle     = -1;
int  ModemFileSize       = -1;
int  GsmTcpUnacknowledgedCount = 0;
bool TcpDataNotification = false;   /* modem has received TCP data */
bool TcpStored           = false;   /* incoming TCP data stored in buffer */

/* received data buffers */
char textSmsReceived[MAX_SMS_LEN];
char textTcpReceived[MAX_SMS_LEN];  /* Incoming TCP format is the same as SMS for this application */
char GNSSData    [GNSS_DATA_SIZE];     /* GNSS data, used in gsmGNSS.c */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* message queue for posting messages to the modem task */
static QueueHandle_t     modemQueue;

/* flag to indicate if the Modem can off GSM Only (By Hardware modification) */
static bool              modemSupportStandAlone = false;

/* flag to indicate if the current action need to be abort */
static bool              modemAbort = false;

/* value to be report for any netowork connection error with reason */
static NetworkConnError_t networkConnErr = NETWORK_NO_ERR;
/* Semaphore to indicate that an abort message is enqueued in the queue*/ 
static SemaphoreHandle_t    abortSemaphore;

/* flag to indicate when modem is powered up */
static eModemPowerState  ModemPowerState = MPS_ALL_OFF;

/* flag to indicate if the Modem Power needs to to be retained */
static bool              ModemPowerRetainNeeded = false;

/* flag to indicate whether the HW uses positive or negative logic for the modem enable line */
static bool              ModemEnablePositive = true;

/* flags to indicate that we are expecting data from the modem, rather than an AT response */
static bool              AtExpectingSmsData  = false;
static bool              AtExpectingImei     = false;
static bool              AtExpectingTcpData  = false;
static bool              AtGSNRetryFlag      = false;

/* Descriptors for data sent to / from the cloud */
static ModemData_t       HttpData;

/* Descriptors for file download from the cloud */
static ModemDownload_t   HttpDownload;

/* Table of expected AT responses and how they are handled */
static ResponseHandler_t ResponseTable[] =
{
/* STRING TO MATCH       FLAG TO SET,           FLAG VALUE,                FUNCTION TO CALL */
  {"CONNECT OK",         &ServerConnected,      true,                      NULL,                     AT_COMPLETE  },
  {"CONNECT FAIL",       &ServerConnected,      false,                     NULL,                     AT_COMPLETE  },
  {"ALREADY CONNECT",    &ServerConnected,      true,                      NULL,                     AT_COMPLETE  },
  {"CONNECT ",           NULL,                  true,                      AtProcessDownloadConnect, AT_IN_PROCESS},
  {"CONNECT",            NULL,                  true,                      NULL,                     AT_COMPLETE  },
  {"OK",                 NULL,                  false,                     NULL,                     AT_COMPLETE  },
  {"ERROR",              NULL,                  false,                     NULL,                     AT_ERROR     },
  {"+CLCC: 1,0,0,0,0",   &callConnected,        true,                      NULL,                     AT_IN_PROCESS},
  {"NO CARRIER",         &callConnected,        false,                     NULL,                     AT_IN_PROCESS},
//  {"+QIREGAPP: \"stream.co.uk\",\"streamip\",\"streamip\"", &apnSet, true, NULL,                     AT_IN_PROCESS},
  {"+QIREGAPP: ",        &apnSet,               true,                      NULL,                     AT_IN_PROCESS},
  {"+CMTI:",             NULL,                  false,                     NULL,                     AT_IN_PROCESS},
  {"+CMGR:",             &AtExpectingSmsData,   true,                      NULL,                     AT_IN_PROCESS},
  {"+CMGL:",             &AtExpectingSmsData,   true,                      NULL,                     AT_IN_PROCESS},
  {"+QGNSSEPO: 1,2",     &EPOEnabled,           true,                      NULL,                     AT_IN_PROCESS},
  {"+QGNSSC: 1",         &GNSSPowerON,          true,                      NULL,                     AT_IN_PROCESS},
  {"+QGNSSC: 0",         &GNSSPowerON,          false,                     NULL,                     AT_IN_PROCESS},
  {"+QGNSSRD: $GNRMC",   NULL,                  false,                     AtProcessPositionInfo,    AT_IN_PROCESS},
  {"+QGNSSRD: $GNVTG",   NULL,                  false,                     AtProcessVelocityInfo,    AT_IN_PROCESS},
  {"+QGNSSRD: $GNGGA",   NULL,                  false,                     AtProcessGGAInfo,         AT_IN_PROCESS},
  {"+QGNSSTS: 1",        &GNSSTimeSync,         true,                      NULL,                     AT_IN_PROCESS},
  {"+QGNSSTS: 0",        &GNSSTimeSync,         false,                     NULL,                     AT_IN_PROCESS},
  {"+QNTP: 0",           &NTPTimeSync,          true,                      NULL,                     AT_IN_PROCESS},
  {"+QNTP: 1",           &NTPTimeSync,          false,                     NULL,                     AT_IN_PROCESS},
  {"+QNTP: 2",           &NTPTimeSync,          false,                     NULL,                     AT_IN_PROCESS},
  {"+QNTP: 3",           &NTPTimeSync,          false,                     NULL,                     AT_IN_PROCESS},
  {"+QNTP: 4",           &NTPTimeSync,          false,                     NULL,                     AT_IN_PROCESS},
  {"+QNTP: 5",           &NTPTimeSync,          false,                     NULL,                     AT_IN_PROCESS},
  {"+CREG: 2,5",         &RegisteredOnNetwork,  true,                      AtProcessNetworkCellID,   AT_IN_PROCESS},  // GSM Registration
  {"+CREG: 2,1",         &RegisteredOnNetwork,  true,                      AtProcessNetworkCellID,   AT_IN_PROCESS},
  {"+CREG: 0,5",         &RegisteredOnNetwork,  true,                      NULL,                     AT_IN_PROCESS},
  {"+QENG: 0,",          &GSMOperatorInfoGot,   true,                      AtProcessGSMNetworkInfo,  AT_IN_PROCESS},  // Country Code, Operator LAC, CID update    
  {"+CREG: 0,1",         &RegisteredOnNetwork,  true,                      NULL,                     AT_IN_PROCESS},
  {"+CREG: 0,0",         &RegisteredOnNetwork,  false,                     NULL,                     AT_IN_PROCESS},
  {"+CREG: 0,2",         &RegisteredOnNetwork,  false,                     NULL,                     AT_IN_PROCESS},
  {"+CREG: 0,3",         &RegisteredOnNetwork,  false,                     NULL,                     AT_IN_PROCESS},
  {"+CREG: 0,4",         &RegisteredOnNetwork,  false,                     NULL,                     AT_IN_PROCESS},
  {"+CGREG: 0,5",        &RegisteredOnGPRS,     true,                      NULL,                     AT_IN_PROCESS},  // GPRS Registration
  {"+CGREG: 0,1",        &RegisteredOnGPRS,     true,                      NULL,                     AT_IN_PROCESS},
  {"+CGREG: 0,0",        &RegisteredOnGPRS,     false,                     NULL,                     AT_IN_PROCESS},
  {"+CGREG: 0,2",        &RegisteredOnGPRS,     false,                     NULL,                     AT_IN_PROCESS},
  {"+CGREG: 0,3",        &RegisteredOnGPRS,     false,                     NULL,                     AT_IN_PROCESS},
  {"+CGREG: 0,4",        &RegisteredOnGPRS,     false,                     NULL,                     AT_IN_PROCESS},
  {"+QFOPEN:",           NULL,                  false,                     AtProcessFileOpen,        AT_IN_PROCESS},
  {"+QFLST:",            NULL,                  false,                     AtProcessFileList,        AT_IN_PROCESS},
  {"+QHTTPDL:",          NULL,                  false,                     NULL,                     AT_COMPLETE  },
  {"+QISACK:",           NULL,                  false,                     AtProcessTcpAck,          AT_IN_PROCESS},
  {"+QBTGATCSCAN:",      NULL,                  false,                     AtProcessBleScan,         AT_IN_PROCESS},
//  {"IPD",                NULL,                  false,                     AtProcessTcpData,         AT_IN_PROCESS},
  {"+QIRDI:",            &TcpDataNotification,  true,                      NULL,                     AT_IN_PROCESS},
  {"+QIRD:",             &AtExpectingTcpData,   true,                      NULL,                     AT_IN_PROCESS},
  {"+CSQ:",              NULL,                  true,                      AtProcessNetworkRSSI,     AT_IN_PROCESS},

  /* State query from Modem */
  {"STATE: IP INITIAL",  &ModemStatus.gprsIdle, true,                      NULL,                     AT_COMPLETE},
  {"STATE: IP START",    &ModemStatus.gprsReg,  true,                      NULL,                     AT_COMPLETE},
  {"STATE: IP CONFIG",   NULL,                  false,                     NULL,                     AT_COMPLETE},
  {"STATE: IP IND",      NULL,                  false,                     NULL,                     AT_COMPLETE},
  {"STATE: IP GPRSACT",  NULL,                  false,                     NULL,                     AT_COMPLETE},
  {"STATE: IP STATUS",   NULL,                  false,                     NULL,                     AT_COMPLETE},
  {"STATE: IP CLOSE",    NULL,                  false,                     NULL,                     AT_COMPLETE},

  /* unsolicited messages - sent after modem powered up */
  {"+CFUN: 1",           NULL,                  false,                     NULL,                     AT_IN_PROCESS},
  {"+CPIN: READY",       NULL,                  false,                     NULL,                     AT_IN_PROCESS},
  {"Call Ready",         NULL,                  false,                     NULL,                     AT_IN_PROCESS},
  {"SMS Ready",          &ModemReady,           true,                      NULL,                     AT_IN_PROCESS},
  {"+CREG: 5",           &RegisteredOnNetwork,  true,                      AtProcessNetworkCellID,   AT_IN_PROCESS},  // GSM Network Cell ID update
  {"+CREG: 1",           &RegisteredOnNetwork,  true,                      AtProcessNetworkCellID,   AT_IN_PROCESS},
  {"+QJDR: JAMMED",      &ModemStatus.Jammed,   true,                      NULL,                     AT_IN_PROCESS},
  {"+QJDR: NO JAMMING",  &ModemStatus.Jammed,   false,                     NULL,                     AT_IN_PROCESS},
  {NULL,                 NULL,                  false,                     NULL,                     AT_NONE      },  // The table must end with a NULL entry
};

/* storage for the message read from queue and then sent via HTTP */
static uint8_t MessageBuffer[MESSAGE_MAX_MESSAGE_SIZE];


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     The MC60E Modem task
 * @param     argument (not used)
 * @return    None
 */
void ModemTask (void const * argument)
{
  ModemMessage_t  message;

  WatchdogSetTimeoutLength(GSMMANAGERTASK, 1000 * 60 * 60);   // Modem task can be busy for extended periods (e.g. making a call).  Set a 1 hour timeout here

  /* create a message queue for this task */
  modemQueue = xQueueCreate(MODEM_QUEUE_LENGTH, sizeof(ModemMessage_t));

  /* create a semaphore for the signal of abort message enqueued */
  abortSemaphore = xSemaphoreCreateBinary();

  /* modem always starts powered down */
  ModemPowerState = MPS_ALL_OFF;

  /* Carry out Wake-Up Self-Test if required */
  PostWakeUpSelfTest(MODEM);

  /* Main task loop */
  for (;;)
  {
    WatchdogSetWaiting(GSMMANAGERTASK);

    /* Wait indefinitely for the next message from the modem queue */
    if (xQueueReceive(modemQueue, &message, portMAX_DELAY) == pdPASS)
    {      
      /* Check if the abort semaphore is set. If so, consume all messages until we find the abort message */ 
      if (xSemaphoreTake(abortSemaphore, 0) == pdTRUE)
      {
        /* Abort signaled, consume all messages until ABORT is found */
        if (message.msgId != MODEM_MSG_ABORT)
        {
          TRACE_MODEM("ModemTask: Flushing message %d\n\r", message.msgId);
          while (xQueueReceive(modemQueue, &message, 0) == pdPASS)
          {
            if (message.msgId == MODEM_MSG_ABORT)
            {
              break;
            }
            else
            {
              TRACE_MODEM("ModemTask: Flushing message %d\n\r", message.msgId);
            }
          }
        }
      }

      /* At this point, we have a valid message to process, including the abort signal */
      WatchdogSetActive(GSMMANAGERTASK);   // NOTE: ModemTask can be busy for extended periods when sending data or making a call. We must also use WatchdoSetActive() in these long functions
      /* now handle the message by state */
      ModemProcessMessage(message);
    }
  }
}




/*
 **************************************************************************************************
 *                                            API
 **************************************************************************************************
 */
/*
 * @brief     Wake up self test
 * @param     none
 * @return    true = self test pass, false = self test fail
 */
bool ModemWUST (void)
{
  //TODO implement self test
  return true;
}


/*
 * @brief     Called from the GSM comms layer when AT response data is received
 * @param     atResponse  The response string received from the modem
 * @return    none
 */
eAtResult ModemReceiveATResponse (char * atResponse)
{
  eAtResult AtResult;

  /* decode the response */
  AtResult = ModemDecodeATResponse(atResponse);

  return AtResult;
}


/*
 * @brief     Called from the GNSS comms layer when a message is received
 * @param     pMessage - pointer to the message data
 * @return    none
 */
void ModemReceiveGnssMessage (char * pMessage)
{
  /* decode the response */
  ModemDecodeGnssMessage(pMessage);
}


/**********************************************************************************/
/*                    API FUNCTIONS CALLED FROM SEQUENCER                         */
/**********************************************************************************/

/*
 * @brief     Send the modem state machine a Power Up message
 * @param     none
 * @return    none
 */
void ModemPowerUp (void)
{
  ModemEnqueueMessage(MODEM_MSG_POWER_ON, NULL);
}


/*
 * @brief     Send the modem state machine a Power Down message for the whole modem (GSM and GNSS)
 * @param     none
 * @return    none
 */
void ModemPowerDown (void)
{
  ModemEnqueueMessage(MODEM_MSG_POWER_OFF, NULL);
}


/*
 * @brief     Send the modem state machine a Power Down message for the GSM section only
 * @param     none
 * @return    none
 */
void ModemPowerDownGsm (void)
{
  ModemEnqueueMessage(MODEM_MSG_POWER_OFF_GSM, NULL);
}


/*
 * @brief     Check the current GSM Info (Lac, Cid, CountryCode, Operator Code) 
 * @param     none
 * @return    none
 */
void ModemCheckGSMInfo (void)
{
  ModemEnqueueMessage(MODEM_MSG_CHK_GSM_INFO, NULL);
}


/**
 * @brief     Make a voice call
 * @param     phoneNumber - telephone number to call
 * @return    none
 */
void ModemMakeVoiceCall (char * pNumber)
{
  ModemEnqueueMessage(MODEM_MSG_VOICE_CALL, (void *)pNumber);
}


/**
 * @brief     Send the specified data via the default protocol (TCP or HTTP)
 * @warning   Data is NOT copied.  The caller MUST maintain the data until the modem
 *            is finished sending, indicated by call toSequencerModemComplete() or SequencerModemError()
 * @param     pData - pointer to the message data to be sent
 * @param     size - number of bytes to send
 * @return    none
 */
void ModemDefaultSend (uint8_t * pData, uint32_t size)
{
  HttpData.pData = pData;
  HttpData.Size = size;

  if (ConfigGetFlag(CONFIG_FLAG_USE_HTTP))
  {
    ModemEnqueueMessage(MODEM_MSG_HTTP_SEND, (void *)&HttpData);
  }
  else
  {
    ModemEnqueueMessage(MODEM_MSG_TCP_SEND, (void *)&HttpData);
  }
}


/**
 * @brief     Send queued messages via the default protocol (TCP or HTTP)
 * @param     priority - identifies the queue to send (high or low priority)
 * @return    none
 */
void ModemDefaultSendQueued (MessagePriority_e priority)
{
  if (ConfigGetFlag(CONFIG_FLAG_USE_HTTP))
  {
    ModemEnqueueMessage(MODEM_MSG_HTTP_SEND_QUEUED, (void *)priority);
  }
  else
  {
    ModemEnqueueMessage(MODEM_MSG_TCP_SEND_QUEUED, (void *)priority);
  }
}


/**
 * @brief     Send the specified data via TCP
 * @warning   Data is NOT copied.  The caller MUST maintain the data until the modem
 *            is finished sending, indicated by call toSequencerModemComplete() or SequencerModemError()
 * @param     pData - pointer to the message data to be sent
 * @param     size - number of bytes to send
 * @return    none
 */
void ModemTcpSend (uint8_t * pData, uint32_t size)
{
  HttpData.pData = pData;
  HttpData.Size = size;
  ModemEnqueueMessage(MODEM_MSG_TCP_SEND, (void *)&HttpData);
}


/**
 * @brief     Send queued messages via TCP
 * @param     priority - identifies the queue to send (high or low priority)
 * @return    none
 */
void ModemTcpSendQueued (MessagePriority_e priority)
{
  ModemEnqueueMessage(MODEM_MSG_TCP_SEND_QUEUED, (void *)priority);
}


/**
 * @brief     Send the specified data via HTTP / HTTPS
 * @warning   Data is NOT copied.  The caller MUST maintain the data until the modem
 *            is finished sending, indicated by call toSequencerModemComplete() or SequencerModemError()
 * @param     pData - pointer to the message data to be sent
 * @param     size - number of bytes to send
 * @return    none
 */
void ModemHttpSend (uint8_t * pData, uint32_t size)
{
  HttpData.pData = pData;
  HttpData.Size = size;
  ModemEnqueueMessage(MODEM_MSG_HTTP_SEND, (void *)&HttpData);
}


/**
 * @brief     Send queued messages via HTTP / HTTPS
 * @param     priority - identifies the queue to send (high or low priority)
 * @return    none
 */
void ModemHttpSendQueued (MessagePriority_e priority)
{
  ModemEnqueueMessage(MODEM_MSG_HTTP_SEND_QUEUED, (void *)priority);
}


/**
 * @brief     Send queued messages via HTTP / HTTPS
 * @param     fileUrl - the URL to download the file from
 * @param     maxSize - maximum data size to download
 * @param     sramAddress - the address is SPI SRAM to put the file in
 * @return    none
 */
void ModemHttpDownload (char * fileUrl, int maxSize, int sramAddress)
{
  HttpDownload.Url = fileUrl;
  HttpDownload.MaxSize  = maxSize;
  HttpDownload.SramAddress = sramAddress;

  ModemEnqueueMessage(MODEM_MSG_HTTP_DOWNLOAD, (void *)&HttpDownload);
}


/**
 * @brief     Start location services
 * @param     none
 * @return    none
 */
void ModemGnssStart (void)
{
  ModemEnqueueMessage(MODEM_MSG_GNSS_START, NULL);
}


/**
 * @brief     Stop location services
 * @param     none
 * @return    none
 */
void ModemGnssStop (void)
{
  ModemEnqueueMessage(MODEM_MSG_GNSS_STOP, NULL);
}


/**
 * @brief     start/Stop gnss jam dtection
 * @param     nMode  : true to switch it on
 * @return    none
 */
void ModemGnssJamControl (bool nMode)
{
  nMode? ModemEnqueueMessage(MODEM_MSG_JAM_SCAN_ON, NULL) : ModemEnqueueMessage(MODEM_MSG_JAM_SCAN_OFF, NULL);
}


/**
 * @brief     Get location, speed and heading from GNSS receiver
 * @param     none
 * @return    none
 */
void ModemGnssGetData (bool storeToSram)
{
  ModemEnqueueMessage(MODEM_MSG_GNSS_READ, (void *)storeToSram);
}


/**
 * @brief   Configure modem audio to use loudspeaker
 * @param   none
 * @return    none
 */
void ModemAudioConfigure (void)
{
  ModemEnqueueMessage(MODEM_MSG_AUDIO_CONFIG, NULL);
}


/**
 * @brief     Play tones from the speaker
 * @param     toneType - type of tone to play
 * @return    none
 */
void ModemAudioPlayTone (eModemTone toneType)
{
  ModemEnqueueMessage(MODEM_MSG_AUDIO_PLAY, (void *)toneType);
}


/**
 * @brief     Stop Play tones from the speaker
 * @param     toneType - type of tone to play
 * @return    none
 */
void ModemAudioPlayToneStop (eModemTone toneType)
{
  ModemEnqueueMessage(MODEM_MSG_AUDIO_STOP, (void *)toneType);
  /* Send cmd directly */
//  GsmStopPlayTone();
}



/**
 * @brief     Send the specified message via SMS
 * @warning   Caller MUST maintain the data until the modem is finished sending
 *            indicated by call toSequencerModemComplete() or SequencerModemError()
 * @param     pMessage - pointer to the message data to be sent
 * @return    none
 */
void ModemSmsSend (char * pMessage)
{
  ModemEnqueueMessage(MODEM_MSG_SMS_SEND, (void *)pMessage);
}


/**
 * @brief     Check for received SMS messages and process
 * @param     pMessage - pointer to the message data to be sent
 * @return    none
 */
void ModemSmsReceive (void)
{
  ModemEnqueueMessage(MODEM_MSG_SMS_RECEIVE, NULL);
}


/**
 * @brief     Perform a bluetooth MAC address scan
 * @param     pMessage - pointer to the message data to be sent
 * @return    none
 */
void ModemBleScan (void)
{
  ModemEnqueueMessage(MODEM_MSG_BLE_SCAN, NULL);
}

/*
 * @brief  Send the CSQ Query to Modem to get the RSSI
 * @param  none
 * @retval None
 */
void ModemGetRSSI (void)
{
  gsmCommsSendAtCommand("AT+CSQ", DEFAULT_AT_TIMEOUT_MS);
}


/**
 * @brief     Set the ModeAbort Flag to stop all modem action,
 *            and enqueue ABORT message to Modem Task for later housekeep
 * @param     None
 * @return    None
 */
void ModemAbort (void)
{
  modemAbort = true;
  ModemEnqueueMessage(MODEM_MSG_ABORT, NULL);
}


/**
 * @brief     Reset the ModemAbort Flag, such that modem communications resume
 * @param     None
 * @return    None
 */
void ModemAbortReset (void)
{
  modemAbort = false;
}


/**
 * @brief     Return if current Modem is Aborted
 * @param     None
 * @return    true if mode is aborted, false if modem is not aborted
 */
bool isModemAbort (void)
{
  return modemAbort;
}


/**
 * @brief     Log the network connection error status.
 * @param     error The error status to set.
 * @return    none
 */
void ModemLogNetworkError(NetworkConnError_t error)
{
  networkConnErr = error;
  /* Log the network error event */
  StatusLogWriteEventCode(STATUS_MODEM_NETWORK_ERR, networkConnErr);
}


/**
 * @brief     Retrieves the current network connection error status.
 * @param     none
 * @return    The current network connection error status
 */
NetworkConnError_t ModemGetNetworkError(void)
{
  return networkConnErr;
}


/**
 * @brief     Set the Properties of Modem, if it support GSM only
 * @param     true if GSM will not off with GNSS, false, GSM and GNSS will off together
 * @return    none
 */
void ModemSupportGSMOffMode (bool support)
{
  modemSupportStandAlone = support;
}


/**
 * @brief     Return if GNSS Off with GSM or not
 * @param     None
 * @return    true if GSM will not off with GNSS, false, GSM and GNSS will off together
 */
bool isModemSupportGSMOffMode (void)
{
  return modemSupportStandAlone;
}


/**
 * @brief     Tell the sequencer that the command has finished
 * @param     result: true = command completed successfully, false = error
 * @return    none
 */
void ModemSequencerDone (bool result)
{
  if (isModemAbort())
  {
    TRACE_MODEM("Modem Aborted, ignore this done message\n\r");
  }
  else
  {
    if (result)
    {
      SequencerModemComplete();
    }
    else
    {
      SequencerModemError();
    }
  }
}


/*
 **************************************************************************************************
 *                            PRIVATE FUNCTIONS - STATE MACHINE RELATED
 **************************************************************************************************
 */


/**
  * @brief  Forward incoming event messages to the current state handler for processing
  * @param  message - event message posted to the modem thread
  * @retval None
  */
static void ModemProcessMessage (ModemMessage_t message)
{
  ModemData_t * pModemData;

  switch (message.msgId)
  {
    case MODEM_MSG_POWER_ON:
      ModemHandlePowerUp();
      break;
    case MODEM_MSG_POWER_OFF:
      ModemHandlePowerDown();
      break;
    case MODEM_MSG_POWER_OFF_GSM:
      ModemHandlePowerDownGsm();
      break;
    case MODEM_MSG_SELF_TEST:
      TRACE_MODEM("Warning - Self-Test not yet implemented\n\r");
      break;
    case MODEM_MSG_CHK_GSM_INFO:
      ModemHandleCheckGSMInfo();
      break;
    case MODEM_MSG_VOICE_CALL:
      ModemHandleMakeVoiceCall((char *)message.parameter);
      break;
    case MODEM_MSG_TCP_SEND:
      pModemData = (ModemData_t *)(message.parameter);
      ModemHandleSendTCP(pModemData->pData, pModemData->Size);
      break;
    case MODEM_MSG_TCP_SEND_QUEUED:
      ModemHandleSendTCPQueued((MessagePriority_e)(message.parameter));
      break;
    case MODEM_MSG_HTTP_SEND:
      pModemData = (ModemData_t *)(message.parameter);
      ModemHandleSendHTTP(pModemData->pData, pModemData->Size);
      break;
    case MODEM_MSG_HTTP_SEND_QUEUED:
      ModemHandleSendHTTPQueued((MessagePriority_e)(message.parameter));
      break;
    case MODEM_MSG_HTTP_DOWNLOAD:
      ModemHandleHTTPDownload((ModemDownload_t *)(message.parameter));
      break;
    case MODEM_MSG_GNSS_START:
      ModemHandleGnssStart();
      break;
    case MODEM_MSG_GNSS_STOP:
      ModemHandleGnssStop();
      break;
    case MODEM_MSG_JAM_SCAN_ON:
      ModemHandleJamControl (true);
      break;
    case MODEM_MSG_JAM_SCAN_OFF:
      ModemHandleJamControl (false);
      break;
    case MODEM_MSG_GNSS_READ:
      ModemHandleGnssGetData((bool)(message.parameter));
      break;
    case MODEM_MSG_AUDIO_CONFIG:
      ModemHandleAudioConfigure();
      break;
    case MODEM_MSG_AUDIO_PLAY:
      ModemHandleAudioPlayTone((eModemTone)message.parameter, 1);
      break;
    case MODEM_MSG_AUDIO_STOP:
      ModemHandleAudioStopTone();
      break;
    case MODEM_MSG_SMS_SEND:
      ModemHandleSmsSend((char *)message.parameter);
      break;
    case MODEM_MSG_SMS_RECEIVE:
      ModemHandleSmsReceive();
      break;
    case MODEM_MSG_BLE_SCAN:
      ModemHandleBleScan();
      break;
    case MODEM_MSG_ABORT:
      ModemHandleModemAbort();
      break;
    default:
      TRACE_MODEM("Error - Unhandled message type %d\n\r", message.msgId);
  }
}


/*
 * @brief     Modem PWRkey io control
 * @param     powerUp : true to power up
 * @return    true : power up
 */
static bool ModemPWRkeyControl (bool powerUp)
{
  /* toggle the modem PowerKey pin to start the modem */
  GpioWritePin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, IO_PIN_SET);
  (powerUp)? sleep(GSM_POWER_ON_DELAY) :sleep(GSM_POWER_OFF_DELAY);
  GpioWritePin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, IO_PIN_RESET);
  sleep(MODEM_BOOT_TIME_MS);
  return (powerUp);
}


/*
 * @brief     Handle modem power up requests from the sequencer.
 *            Report back to the sequencer with success / failure indication
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandlePowerUp (void)
{
  /* Make sure modem is fully powered */
  if (ModemPowerOn())
  {
    ModemSequencerDone(true);
  }
  else
  {
    ModemSequencerDone(false);
  }

  return true;
}


/*
 * @brief     Check if the modem is fully powered for all-in-one mode, and power up if not.
 *            For stand-alone Modem HW design, the GNSS could be on/off depend on the flag.
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemPowerOn (void)
{
  /* clear abort flag */
  ModemAbortReset();
  /* Check if GSM already powered */
  if ((MPS_ALL_ON == ModemPowerState) || (MPS_GSM_ON_GNSS_OFF == ModemPowerState))
  {
    TRACE_MODEM("Modem GSM power already ON\n\r");
    return true;
  }

  /* if battery state is critical, then we must not power up */
  if (POWER_STATE_CRITICAL == BatteryMonitorGetPowerState())
  {
    TRACE_MODEM("Modem power up not allowed - Battery critical\n\r");
    return false;
  }

  TRACE_MODEM("Powering up the modem\n\r");
  ModemReady = false;

  /* if modem is fully powered down (GNSS + GSM OFF) then we reset the statistics and configure GPIO pins */
  if (MPS_ALL_OFF == ModemPowerState)
  {
    /* clear ModemStatus */
    ModemStatus.IMEI = 0;
    ModemStatus.GPSTimeToFix = 0;
    ModemStatus.GsmLac = 0;
    ModemStatus.GsmCid = 0;
    ModemStatus.countryCode = 0;
    ModemStatus.operatorCode = 0;
    ModemStatus.RSSI = 0;
    ModemStatus.Jammed = false;
    /* Initialise modem GPIO pins */
    ModemGpioInit();
  }

  /* Turn on power supply to the GSM section */
  ModemGSMPowerControl(true);

 /* toggle the modem PowerKey pin to start the modem */
  GpioWritePin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, IO_PIN_SET);
  sleep(GSM_POWER_ON_DELAY);
  GpioWritePin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, IO_PIN_RESET);
  sleep(MODEM_BOOT_TIME_MS);

  /* enable Modem comms so we can capture the output */
  gsmCommsInitialiseModem();

  /* wait for the modem to finish booting */
  unsigned int timeout = 10000;
  while ((!ModemReady) && (timeout))
  {
    sleep(100);
    timeout = timeout - 100;
  }

  if (!ModemReady)
  {
    TRACE_MODEM("ERROR - no response from Modem\n\r");
    ModemLogNetworkError(NETWORK_MODEM_ERR);
    return false;
  }

  /* get the IMEI */
  ModemUpdateIMEI();
  /* repeat once if read value is invalid and retry flag is up */
  if (AtGSNRetryFlag)
  {
    ModemUpdateIMEI();
  }
  /* get the IMSI */
  gsmCommsSendAtCommand("AT+CIMI", DEFAULT_AT_TIMEOUT_MS);
  #if MODEM_LOG_MODULE_VERSION
    gsmCommsSendAtCommand("AT+CGMM", DEFAULT_AT_TIMEOUT_MS);
    gsmCommsSendAtCommand("AT+CGMR", DEFAULT_AT_TIMEOUT_MS);
  #endif
  #if MODEM_LOG_RSSI
     gsmCommsSendAtCommand("AT+QEXTUNSOL=\"SQ\",1", DEFAULT_AT_TIMEOUT_MS);
  #endif
  #if MODEM_JAMMING_DETECTION
    gsmCommsSendAtCommand("AT+QJDCFG=\"urc\",1", DEFAULT_AT_TIMEOUT_MS);
    gsmCommsSendAtCommand("AT+QJDR=1", DEFAULT_AT_TIMEOUT_MS);
  #endif
  #if MODEM_COUNTRY_CODE_CHECK
    gsmCommsSendAtCommand("AT+QENG=1", DEFAULT_AT_TIMEOUT_MS);
  #endif
  if (ConfigGetFlag(CONFIG_FLAG_CUSTOM_TONE))
  {
    ModemHandleToneUpload(MODEM_TONE_CUSTOM_EMERGENCY_START);
  }

  if (isModemSupportGSMOffMode())  /* for modem stand alone mode */
  {
    if (ModemPowerState == MPS_GSM_OFF_GNSS_ON)
    {
      ModemPowerState = MPS_ALL_ON;
    }
    else if (ModemPowerState == MPS_ALL_OFF)
    {
      ModemPowerState = MPS_GSM_ON_GNSS_OFF;
    }
  }
  else
  {
    ModemPowerState = MPS_ALL_ON;  /* for modem all-in-one power mode */
  }
  return true;
}


/*
 * @brief     GSM VCC power control
 * @param     PowerEnable   : true to enable gsm power switch
 * @return    true on success, false on failure
 */
static void ModemGSMPowerControl (bool PowerEnable)
{
  if (PowerEnable)
  {
    /* turn on power to the modem, or GSM if GNSS was controlled in stand alone mode */
    if (ModemEnablePositive)
    {
     GpioWritePin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, IO_PIN_SET);
    }
    else
    {
     GpioWritePin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, IO_PIN_RESET);
    }
    sleep(10);
  }
  else
  {
    /* turn off power to the modem, or GSM if GNSS was controlled in stand alone mode */
    if (ModemEnablePositive)
    {
     GpioWritePin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, IO_PIN_RESET);
    }
    else
    {
     GpioWritePin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, IO_PIN_SET);
    }
  }
}


/**
 * @brief     Mark if Modem Power need to be retain, Power Down is not allowed after set
 * @param     None
 * @return    None
 */
void ModemMarkPowerRetainNeeded (bool retain)
{
  ModemPowerRetainNeeded = retain;
}


/*
 * @brief     Power down checking
 * @param     none
 * @return    true or false to power down the modem
 */
static bool ModemPowerDownAllowed (void)
{
  return ( !ModemPowerRetainNeeded && (!SeqJourneyInProgress ()) );
}


/*
 * @brief     Power down the whole modem (GSM and GNSS)
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandlePowerDown (void)
{
  if ( (MPS_ALL_OFF != ModemPowerState) && (ModemPowerDownAllowed ()) )
  {
    TRACE_MODEM("Modem power down all\n\r");

    /* turn off power supply to the GNSS section now */
    if (modemSupportStandAlone)
    {
      GpioWritePin(GNSS_POWER_ENABLE_PORT, GNSS_POWER_ENABLE_PIN, IO_PIN_RESET);  /* GNSS power off */
    }

    /* turn off power supply to the GSM section */
    ModemGSMPowerControl(false);

    /* tri-state gpio used by the modem to reduce power draw */
    ModemGpioDeInit();

    /* update the internal modem power state */
    ModemPowerState = MPS_ALL_OFF;
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(true);

  return true;
}


/*
 * @brief     Power down the GSM section only
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandlePowerDownGsm (void)
{
  if (((MPS_ALL_ON == ModemPowerState) || (MPS_GSM_ON_GNSS_OFF == ModemPowerState)) && (!ModemPowerRetainNeeded))
  {
    TRACE_MODEM("Modem power down GSM only\n\r");

    /* Turn off power supply to the GSM section only (GNSS remains ON) */
    ModemGSMPowerControl(false);

    /* update the internal modem power state */
    if (MPS_ALL_ON == ModemPowerState)
    {
      ModemPowerState = MPS_GSM_OFF_GNSS_ON;
    }
    else if (MPS_GSM_ON_GNSS_OFF == ModemPowerState)
    {
      ModemPowerState = MPS_ALL_OFF;
    }

    /* tell the sequencer we are finished */
    ModemSequencerDone(true);
  }
  return true;
}


/**
 * @brief     Upload the audio buffer to Modem specified by the Modem Tone
 * @param     eModemTone modemTone
 * @return    None
 */
static bool ModemHandleToneUpload (eModemTone modemTone)
{
  bool result = false;
  switch (modemTone)
  {
  case MODEM_TONE_CUSTOM_EMERGENCY_START:
    result = GsmAudioBufferTransferToModem(custom_start_buf, RIP_CUSTOM_START_FILE_NAME, RIP_CUSTOM_START_FILE_SIZE);
    break;
  case MODEM_TONE_CUSTOM_EMERGENCY_CANCEL:
    result = GsmAudioBufferTransferToModem(custom_cancel_buf, RIP_CUSTOM_CANCEL_FILE_NAME, RIP_CUSTOM_CANCEL_FILE_SIZE);
    break;
  default:
    break;
  }

  return result;
}


/**
 * @brief     Check the GSM Network Connection Info
 * @param     none
 * @return    true if modem process the request success, false otherwise
 */
static bool ModemHandleCheckGSMInfo (void)
{
  TRACE_MODEM("Modem Handle Check GSM Info\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Check the GSM Network Registration and the GSM Network Info*/
  bool result = GsmUtilsNetworkInfoGet(GSM_REGISTER_TIMEOUT_SECS);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Make a voice call
 * @param     phoneNumber - telephone number to call
 * @return    none
 */
static bool ModemHandleMakeVoiceCall (char * pNumber)
{
  TRACE_MODEM("Make voice call\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Configure the modem audio output */
  GsmAudioConfigure(AUDIO_CHANNEL_LOUDSPEAKER, 85);

  /* make the voice call */
  bool result = GsmVoiceCall(pNumber);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Send the specified data via TCP
 * @param     pData - pointer to the message data to be sent
 * @param     size - number of bytes to send
 * @return    true on success, false on failure
 */
static bool ModemHandleSendTCP (uint8_t * pData, uint32_t size)
{
  bool result = true;

  TRACE_MODEM("Send single message via TCP\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(GPRS_DEFAULT_PDP_CONTEXT, true);

  /* Send the message */
  if (result)
  {
    GsmTcpConfigAndSend(pData, size);
  }

  /* disconnect GPRS */
  if (result)
  {
    result = GsmGprsDisconnect(GPRS_DEFAULT_PDP_CONTEXT);
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return (result);
}


/**
 * @brief     Set up a GPRS connection and then send queued messages via TCP
 *            NOTE: Messages from the high priority queue are ALWAYS sent.  Low priority
 *                  message are only sent if specified in the priority parameter
 * @param     priority - The queue to send (high or low priority)
 *                       High is used for emergency data
 *                       Low is used for all other data
 * @return    true on success, false on failure
 */
static bool ModemHandleSendTCPQueued (MessagePriority_e priority)
{
  bool result = true;

  TRACE_MODEM("Send Queued messages via TCP\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(GPRS_DEFAULT_PDP_CONTEXT, true);

  /* Configure the HTTP session */
  if (result)
  {
    result = GsmTcpConfigure();
  }

  if (ServerConnected)
  {
    /* Always send from HIGH priority queue first */
    if (result)
    {
      result = ModemSpoolFromQueueTCP(MESSAGE_PRIORITY_HIGH);
    }

    /* If requested, we also send from the LOW priority queue */
#ifdef MODEM_ALWAYS_SEND_BOTH_QUEUES
    if (result)
#else
    if ((result) && (MESSAGE_PRIORITY_LOW == priority))
#endif
    {
      result = ModemSpoolFromQueueTCP(MESSAGE_PRIORITY_LOW);
    }
  }

  /* disconnect GPRS */
  result &= GsmGprsDisconnect(GPRS_DEFAULT_PDP_CONTEXT);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return (result);
}


/**
 * @brief     Spool all messages from the specified queue via TCP
 *            NOTE: The GPRS connection MUST be set up and connected before calling this function
 * @param     priority - The queue to send (high or low priority)
 *                       High is used for emergency data
 *                       Low is used for all other data
 * @return    true on success, false on failure
 */
static bool ModemSpoolFromQueueTCP (MessagePriority_e priority)
{
  bool result = true;
  int Size;

  /* Send all messages in the specified queue */
  while ((MessageQueueCount(priority)) && (result))
  {
    /* get the next message from the queue */
    MessageQueueRead(priority, MessageBuffer, &Size);

    /* send the message via TCP */
    result = GsmTcpSend(MessageBuffer, Size);

    /* if the message is acknowledged, remove it from the queue */
    if (result)
    {
      MessageQueueRemove(priority);
    }
  }
  return result;
}


/**
 * @brief     Send the specified data via HTTP / HTTPS
 * @param     pData - pointer to the message data to be sent
 * @param     size - number of bytes to send
 * @return    true on success, false on failure
 */
static bool ModemHandleSendHTTP (uint8_t * pData, uint32_t size)
{
  bool result = true;

  TRACE_MODEM("Send single message via HTTP(S)\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(GPRS_DEFAULT_PDP_CONTEXT, true);

  /* Send the message */
  if (result)
  {
    GsmHttpConfigAndSend(pData, size);
  }

  /* disconnect GPRS */
  if (result)
  {
    result = GsmGprsDisconnect(GPRS_DEFAULT_PDP_CONTEXT);
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return (result);
}


/**
 * @brief     Set up a GPRS connection and then send queued messages via HTTP / HTTPS.
 *            NOTE: Messages from the high priority queue are ALWAYS sent.  Low priority
 *                  message are only sent if specified in the priority parameter
 * @param     priority - The queue to send (high or low priority)
 *                       High is used for emergency data
 *                       Low is used for all other data
 * @return    true on success, false on failure
 */
static bool ModemHandleSendHTTPQueued (MessagePriority_e priority)
{
  bool result = true;

  TRACE_MODEM("Send Queued messages via HTTP(S)\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(GPRS_DEFAULT_PDP_CONTEXT, true);

  /* Configure the HTTP session */
  if (result)
  {
    result = GsmHttpConfigure();
  }

  /* Always send from HIGH priority queue first */
  if (result)
  {
    result = ModemSpoolFromQueueHTTP(MESSAGE_PRIORITY_HIGH);
  }

  /* If requested, we also send from the LOW priority queue */
#ifdef MODEM_ALWAYS_SEND_BOTH_QUEUES
    if (result)
#else
    if ((result) && (MESSAGE_PRIORITY_LOW == priority))
#endif
  {
    result = ModemSpoolFromQueueHTTP(MESSAGE_PRIORITY_LOW);
  }

  /* disconnect GPRS */
  result &= GsmGprsDisconnect(GPRS_DEFAULT_PDP_CONTEXT);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return (result);
}


/**
 * @brief     Spool all messages from the specified queue via HTTP / HTTPS.
 *            NOTE: The GPRS connection MUST be set up and connected before calling this function
 * @param     priority - The queue to send (high or low priority)
 *                       High is used for emergency data
 *                       Low is used for all other data
 * @return    true on success, false on failure
 */
static bool ModemSpoolFromQueueHTTP (MessagePriority_e priority)
{
  bool result = true;
  int Size;

  /* Send all messages in the specified queue */
  while ((MessageQueueCount(priority)) && (result))
  {
    /* get the next message from the queue */
    MessageQueueRead(priority, MessageBuffer, &Size);

    /* send the message via HTTP POST */
    result = GsmHttpSend(MessageBuffer, Size);

    /* if the message is acknowledged, remove it from the queue */
    if (result)
    {
      MessageQueueRemove(priority);
    }
  }
  return result;
}


/**
 * @brief     Download a file via HTTP / HTTPS
 * @param     pModemDownload - pointer to the download information
 * @return    true on success, false on failure
 */
static bool ModemHandleHTTPDownload (ModemDownload_t * pModemDownload)
{
  bool result = true;

  TRACE_MODEM("Download a file via HTTP(S)\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(GPRS_DEFAULT_PDP_CONTEXT, true);

  /* Download the file */
  if (result)
  {
    result = GsmHttpDownloadFile(pModemDownload->Url, pModemDownload->MaxSize, pModemDownload->SramAddress);
  }

  /* disconnect GPRS */
  if (result)
  {
    result = GsmGprsDisconnect(GPRS_DEFAULT_PDP_CONTEXT);
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return (result);
}


/**
 * @brief     Start location services
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandleGnssStart (void)
{
  bool result = true;

  TRACE_MODEM("Start GNSS\n\r");

  result = GsmGnssStart();
  if (isModemSupportGSMOffMode())
  {
    if (ModemPowerState == MPS_GSM_ON_GNSS_OFF)
    {
      ModemPowerState = MPS_ALL_ON;  /* for modem Stand alone mode */
    }
  }
  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Stop location services
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandleGnssStop (void)
{
  bool result = true;

  TRACE_MODEM("Stop GNSS\n\r");

  result = GsmGnssStop();
  if (result)
  {
    if (ModemPowerState == MPS_ALL_ON)
    {
      ModemPowerState = MPS_GSM_ON_GNSS_OFF;
    }
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     enable/disable GNSS jamming scan function
 * @param     bMode  : true to enable the scan
 * @return    true on success, false on failure
 */
static bool ModemHandleJamControl (bool bMode)
{
  bool result = false;
  if (ConfigGetFlag(CONFIG_FLAG_GNSS_JAMMING_DETECT) && bMode)
  {
    result = GsmGnssJamDetectionControl (bMode);
  }
  else
  {
    result = GsmGnssJamDetectionControl (false);
  }
  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Start location services
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandleGnssGetData (bool storeToSram)
{
  bool result = false;

  TRACE_MODEM("Get GNSS data\n\r");

  /*
   * When not in journey check whether we are able to get GNSS valid data from Satellites
   * and retry once per second for fixed period
   */
  for (int timeout = 0; timeout < ((MotionMonitorGetState() != IN_JOURNEY) ? GNSS_GETVALIDDATA_TIMEOUT_SECS : GNSS_GETDATA_TIMEOUT_SECS) ; timeout++)
  {
    result = GsmGnssGetData(storeToSram);
    if (isModemAbort())
    {
      break;
    }

    if (MotionMonitorGetState() != IN_JOURNEY)
    {
      /* Only proceed if GNSS Data is valid when not in journey */
      if (result && gnssDataValid)
      {
        break;
      }
    }
    else if (result)
    {
      break;
    }
    sleep(ONE_SECOND_IN_MS);
  }

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief   Configure modem audio to use loudspeaker
 * @param   none
 * @return  true on success, false on failure
 */
static bool ModemHandleAudioConfigure (void)
{
  TRACE_MODEM("Configure audio\n\r");

  bool result = GsmAudioConfigure(AUDIO_CHANNEL_LOUDSPEAKER, 85);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Play tones from the speaker
 * @param     toneType - type of tone to play
 * @param     uint8_t mode, start or stop play
 * @return    true on success, false on failure
 */
static bool ModemHandleAudioPlayTone (eModemTone toneType, uint8_t mode)
{
  bool result = false;

  TRACE_MODEM("Play tones\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* Configure the modem audio output */
  GsmAudioConfigure(AUDIO_CHANNEL_LOUDSPEAKER, 85);

  switch(toneType)
  {
  case MODEM_TONE_EMERGENCY:
    result = GsmAudioRepeatingTone(mode, 500, 500, 500, 12);
    break;
  case MODEM_TONE_LOW_BATTERY:
    result = GsmAudioSingleTone(1000, 250);
    break;
  case MODEM_TONE_CUSTOM_EMERGENCY_START:
    result = GsmAudioPlayFile(RIP_CUSTOM_START_FILE_NAME);
    if (!result)
    {
      result = GsmAudioRepeatingTone(mode, 500, 500, 500, 12);
    }
    break;
  case MODEM_TONE_CUSTOM_EMERGENCY_CANCEL:
    /* Custom Emergency Cancel File will only upload on request */
    ModemHandleToneUpload(MODEM_TONE_CUSTOM_EMERGENCY_CANCEL);

    /* Request to play regardless the upload result, as the file may already in RAM */
    result = GsmAudioPlayFile(RIP_CUSTOM_CANCEL_FILE_NAME);
    break;
  }
  if (toneType != MODEM_TONE_CUSTOM_EMERGENCY_CANCEL)
  {
    /* tell the sequencer we are finished */
    ModemSequencerDone(result);
  }
  else
  {
    /*  not required to notify the sequencer for the cancel case  as if ModemHandleAudioStopTone */
  }

  return result;
}


/**
 * @brief     Stop playing tones from the speaker
 * @param     none
 * @return    true on success, false on failure
 */
static bool ModemHandleAudioStopTone (void)
{
  bool result = false;

  TRACE_MODEM("Stop tones\n\r");
  GsmAudioStopTones();

  return result;
}


/**
 * @brief     Send the specified message via SMS
 * @param     pMessage - pointer to the message data to be sent
 * @return    true on success, false on failure
 */
static bool ModemHandleSmsSend (char * pMessage)
{
  TRACE_MODEM("Send SMS message\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* send the message */
  bool result = GsmSmsSend(ConfigGetSmsNumber(), pMessage);

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Check for received SMS messages and process
 * @param     pMessage - pointer to the message data to be sent
 * @return    true on success, false on failure
 */
static bool ModemHandleSmsReceive (void)
{
  TRACE_MODEM("Check and process received SMS messages\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* send the message */
  bool result = GsmSmsReceive();

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}


/**
 * @brief     Perform a bluetooth MAC address scan
 * @param     pMessage - pointer to the message data to be sent
 * @return    true on success, false on failure
 */
static bool ModemHandleBleScan (void)
{
  TRACE_MODEM("Perform Bluetooth scan\n\r");

  /* Check modem is fully powered */
  ModemPowerOn();

  /* send the message */
  bool result = GsmBleScan();

  /* tell the sequencer we are finished */
  ModemSequencerDone(result);

  return result;
}

/**
 * @brief     Housekeep the modem for the abort action
 * @param     pMessage - none
 * @return    true on success, false on failure
 */
static bool ModemHandleModemAbort (void)
{

  TRACE_MODEM("Perform Abort Action\n\r");

  /* only clear the modem states when GSM is on */
  if ((ModemPowerState == MPS_ALL_ON) || (ModemPowerState == MPS_GSM_ON_GNSS_OFF))
  {
    /* release the abort flag to allow housekeeping action for Modem to complete */
    modemAbort = false;

    /* Flush the Modem to push it back to AT command Mode */
    GsmTcpFlush();

    /* Close any TCP/IP connection before */
    GsmTcpClose();

    /* Stop BLE Scanning */
    GsmStopBleScan();

    /* set the abort flag back */
    modemAbort = true;
  }

  return true;
}


/*
 **************************************************************************************************
 *                                PRIVATE FUNCTIONS - MODEM RELATED
 **************************************************************************************************
 */
/*
 * @brief  Decode and AT response message from the modem.  In some cases we also send an event
 *         message to the modem task to indicate that something has happened
 * @param  atResponse  The response string received from the modem
 * @retval None
 */
static eAtResult ModemDecodeATResponse (char * pAtResult)
{
  eAtResult AtResult = AT_IN_PROCESS;
  int index = 0;

  /* if we are waiting for SMS data */
  if (true == AtExpectingSmsData)
  {
    AtExpectingSmsData = false;
    AtProcessSMSRxData(pAtResult);
  }
  else if (true == AtExpectingImei)
  {
    AtExpectingImei = false;
    AtProcessImeiData(pAtResult);
  }
  else if (true == AtExpectingTcpData)
  {
    AtExpectingTcpData = false;
    AtProcessTcpRxData(pAtResult);
  }
  else
  {
    /* scan the response table looking for a match */
    while (NULL != ResponseTable[index].ResponseString)
    {
      if (strstr(pAtResult, ResponseTable[index].ResponseString) != NULL)
      {
        /*
         * We have matched the string, so we now act on it according to the table entry
         * First we check to see if there is flag to set
         */
        if (NULL != ResponseTable[index].Flag)
        {
          /* set the flag value */
          *ResponseTable[index].Flag = ResponseTable[index].FlagValue;
        }

        /* Next we check to see if there is a handler function to call */
        if (NULL != ResponseTable[index].pHandlerFunc)
        {
          /* call the handler */
          ResponseTable[index].pHandlerFunc(pAtResult);
        }

        /* get the AT result code */
        AtResult = ResponseTable[index].AtResultCode;
        break;
      }
      index++;
    }
  }
  return AtResult;
}


/*
 * @brief   Process an AT incoming SMS message from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessSMSRxData (char * pMessage)
{
  /*
   * For now we can only deal with one incoming SMS at a time
   * If we need to deal with multiple messages in future, we could store them in SPI SRAM for processing
   */
  if (false == smsStored)
  {
    strncpy(textSmsReceived, pMessage, MAX_SMS_LEN);
    textSmsReceived[MAX_SMS_LEN-1] = '\0';
    smsStored = true;
  }
}


/*
 * @brief   Process an IMEI data from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessImeiData (char * pMessage)
{
  uint64_t IMEI = 0;

  /* get the length of the data */
  int digits = strlen(pMessage);
  int digitValue = 0;
  int topDigit   = 0;
  uint8_t digitFlag = 0;

  /* convert string of ASCII digits to 64 bit integer */
  for (int i = 0; i < digits; i++)
  {
    IMEI = IMEI * 10;
    digitValue = (pMessage[i] - ASCII_ZERO);
    IMEI += digitValue;
    if ((digitValue < 0) || (digitValue > 9)) /* check must be digit */
    {
      digitFlag++;
    }
    if (i == 0)
    {
      topDigit = digitValue;  /* get top digit */
    }
  }
  /* its-066, check all digit, top digit must be a 8, 15 digits long */
  if ((digitFlag > 0) || (topDigit != 8) || (digits != 15))
  {
    if (!AtGSNRetryFlag)
    {
      ConsolePrintf("%s, %08lX%08lX [%d][%d][%d]\n\r", pMessage, (uint32_t)((IMEI>>32) & 0xFFFFFFFF), (uint32_t)(IMEI & 0xFFFFFFFF),
                    digitFlag, topDigit, digits);
      AtGSNRetryFlag = true;  /* only try once or the previous IMEI will be kept */
      StatusLogWriteEventCode(STATUS_MODEM_IMEI_RETRY, STATUS_DATA_LOAD_NONE);
    }
    else
    {
      AtGSNRetryFlag = false;  /* retry only once */
    }
  }
  else
  {
    /* copy to the modem status store */
    ModemStatus.IMEI = IMEI;
    AtGSNRetryFlag = false;  /* reset flag */
  }
}

/*
 * @brief   Process an AT GNSS NMEA/GGA message from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessGGAInfo (char * pMessage)
{
  char *ret = strstr(pMessage,"$GNGGA");
  strcpy(GNSSData, ret);
  GNSSDataStored = true;
}


/*
 * @brief   Process an AT GNSS position message from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessPositionInfo (char * pMessage)
{
  char *ret = strstr(pMessage,"$GNRMC");
  strcpy(GNSSData, ret);
  GNSSDataStored = true;
}


/*
 * @brief   Process an AT Velocity position message from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessVelocityInfo (char * pMessage)
{
  /* copy GNSS information */
  char *ret = strstr(pMessage,"$GNVTG");
  strcpy(GNSSData, ret);
  velocityStored = true;
}


/*
 * @brief   Process an AT Network registration message from the modem to obtain Network LAC & CID
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessNetworkCellID (char * pMessage)
{
  char cellLAC[5] = { 0 };
  char cellCID[5] = { 0 };

  /* copy Network information */
  char *ret = strstr(pMessage,"+CREG:");
  ret = strstr(ret,"\"");
  if (sscanf(ret,"\"%[^,],\"%[^\"]\"", cellLAC, cellCID) == 2)
  {
    char *ptr;
    long lac,cid;

    lac = strtol(cellLAC, &ptr, 16);
    cid = strtol(cellCID, &ptr, 16);

    /* copy to the modem LAC and CID status store */
    ModemStatus.GsmLac = (uint16_t) lac;
    ModemStatus.GsmCid = (uint16_t) cid;
  }
}


/*
 * @brief   Process an AT CSQ message from the modem to obtain RSSI
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessNetworkRSSI (char * pMessage)
{
  char rssiStr[3] = {0}; /* Buffer to store extracted RSSI value as a string */
  int rssi = -1;         /* Initialize RSSI to an invalid value */

  /* copy Network information */
  char *ret = strstr(pMessage,"+CSQ:");
  if (ret != NULL)
  {
    ret = ret + 6; // Move to RSSI value
    if (sscanf(ret, "%2[^,]", rssiStr) == 1)
    {
      rssi = atoi(rssiStr);
      ModemStatus.RSSI = (uint16_t) rssi;
    }
  }
}


/*
 * @brief   Process an AT QEN message from the modem to obtain country code, operator code, LAC, CID
 *          Sample String: +QENG: 0,460,00,1806,2602,64,46,-72,119,119,5,8,x,x,x,x,x,x,x
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessGSMNetworkInfo (char * pMessage)
{
  char countryCodeStr[4] = {0};   /* 3-byte country code as a decimal string */
  char operatorCodeStr[4] = {0};  /* 3-byte network operator code as a decimal string */
  char cellLAC[5] = { 0 };        /* 4-byte cell LAC as a hex string */
  char cellCID[5] = { 0 };        /* 4-byte cell CID as a hex string */

  /* copy Network information */
  char *ret = strstr(pMessage,"+QENG: 0,");
  if (ret != NULL)
  {
    /* Move to the numbers after +QENG: 0, */
    ret += strlen("+QENG: 0,");

    /* sscanf to extract MCC and MNC (2nd and 3rd fields) */
    /* sscanf to extract LAC and CID (4th and 5th fields) */
    int countryCode = -1, operatorCode = -1;
    long lac =-1 ,cid = -1;

    if (sscanf(ret, " %3[^,],%3[^,],%4[^,],%4[^,]", countryCodeStr, operatorCodeStr, cellLAC, cellCID) == 4)
    {
      char *ptr;

      /* copy info to the Modem status store */
      countryCode = atoi(countryCodeStr);
      operatorCode = atoi(operatorCodeStr);

      lac = strtol(cellLAC, &ptr, 16);
      cid = strtol(cellCID, &ptr, 16);
      
      ModemStatus.countryCode = (uint16_t) countryCode;
      ModemStatus.operatorCode = (uint16_t) operatorCode;
      ModemStatus.GsmLac = (uint16_t) lac;
      ModemStatus.GsmCid = (uint16_t) cid;
    }
  }
}


/*
 * @brief   Process an AT File Open message from the modem.  We need to store the file handle to be used
 *           in subsequent file operations
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessFileOpen (char * pMessage)
{
  sscanf(pMessage, "+QFOPEN: %d", &ModemFileHandle);
}


/*
 * @brief   Process an AT File list message from the modem.  We need to store the file size to be used
 *           in subsequent file operations
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessFileList (char * pMessage)
{
  /* find the file size in the string */
  char *ret = strstr(pMessage,"+QFLST:");
  ret = strstr(ret,",");

  /* try to read and decode it */
  if (sscanf(ret, ",%d", &ModemFileSize) != 1)
  {
    ModemFileSize = -1;
  }
}


/*
 * @brief   Process the AT "CONNECT" message received when performing HTTP file download.  We need to
 *           decode the data size parameter so we know how much binary data follows
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessDownloadConnect (char * pMessage)
{
  int BinaryDataLength;

  /* read the data size from the string */
  sscanf(pMessage, "CONNECT %d", &BinaryDataLength);

  /* Put the AT layer into RxBinary mode */
  gsmCommsBinaryMode(BinaryDataLength);
}


/*
 * @brief   Process a TCP ACK from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessTcpAck (char * pMessage)
{
  int Dummy1;
  int Dummy2;
  sscanf(pMessage, "+QISACK: %d,%d,%d", &Dummy1, &Dummy2, &GsmTcpUnacknowledgedCount);
}


/*
 * @brief   Process incoming TCP data from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessTcpRxData (char * pMessage)
{
  if (false == TcpStored)
  {
    strncpy(textTcpReceived, pMessage, MAX_SMS_LEN);
    textTcpReceived[MAX_SMS_LEN-1] = '\0';
    TcpStored = true;
  }
}


/*
 * @brief   Process the Bluetooth scan result message from the modem
 * @param   msgString   Pointer to the message that was received from the modem
 * @retval  none
 */
static void AtProcessBleScan (char * pMessage)
{
  GsmBleDecodeScanResult(pMessage);
}


/*
 * @brief  Decode a GNSS message from the modem (direct from the GNSS subsystem, not from the main modem serial port)
 * @param  pMessage  The message received from the GNSS subsystem on the modem
 * @retval None
 */
static void ModemDecodeGnssMessage (char * pMessage)
{
  /*
   * Flag to make sure RMC and GGA decodes are in sync before we store data to SRAM
   */
  static bool GNRMCReceived = false;
  static char GNRMCBuf[GNSS_DATA_SIZE];

  if (strstr(pMessage, "$GNRMC") != NULL)
  {
    /* 
     * ITS-131: Decoding the GNRMC sentence immediately may cause missing characters in 
     * the GNGGA reception.
     * Suspected the decoding process leading to missed UART interrupts.(floating point calculation)
     * Root cause investigation is needed. 
     */    

    /* store up the the RMC message for decoding after GGA received */
    strcpy(GNRMCBuf, pMessage);
    GNRMCReceived = true;
  }
  else if (strstr(pMessage, "$GNGGA") != NULL)
  {
    /* handle the stored RMC message, and decode the GNSS data(valid or invalid) */
    bool rmcResult = (GNRMCReceived && GsmGnssDecodeMessageRMC(GNRMCBuf));
    /* handle the GGA message for update SAT number before save to PSRAM */
    GsmGnssDecodeMessageGGA(pMessage);
    /* check we have received RMC and GGA messages in sequence */
    if (rmcResult && (SeqJourneyInProgress()))   /* save to RAM for valid and invalid gnss data during the journey */
    {
      /* store the data to SPI SRAM */
      GnssEventSaveToSram();
    }
    GNRMCReceived = false;
  }
  else if (strstr(pMessage, "$PMTKSPF") != NULL)
  {
    /* update jamming status */
    GsmGnssDecodeMessageMTK(pMessage);
  }
  else if (strstr(pMessage, "$GPGSV") != NULL)
  {
    if (TestModeGet())
    {
      /*
       * handle the GSV message during factory test
       * Note - for simplicity we only process GPS satellites for now (GPGSV) and not BeiDou (BDGSV), Glonass (GLGSV), or Galileo (GAGSV)
       */
      GsmGnssDecodeMessageGSV(pMessage);
    }
  }

  if (SeqJourneyInProgress())
  {
    /* if in Journey, need to update some system status to the cloud for the GNSS time */
    GsmGnssInitialFixStatusCheck();
  }
}


/*
 * @brief  Add a new message to the modem task message queue
 * @param  msgId     the message ID to send
 * @param  parameter parameter of unknown type
 * @retval None
 */
static void ModemEnqueueMessage (uint8_t msgId, void * parameter)
{
  ModemMessage_t message;

  message.msgId = msgId;
  message.parameter = parameter;
  if (message.msgId == MODEM_MSG_ABORT)
  {
    /* Signal semaphore that ABORT is queued */
    xSemaphoreGive(abortSemaphore);
  }
  xQueueSendToBack(modemQueue, &message, portMAX_DELAY);
}


/*
 * @brief  raed IMEI from modem during power up
 * @param  none
 * @retval None
 */
static void ModemUpdateIMEI (void)
{
  AtExpectingImei = true;
  gsmCommsSendAtCommand("AT+GSN", DEFAULT_AT_TIMEOUT_MS);
}


/*
 **************************************************************************************************
 *                 PRIVATE FUNCTIONS - HARDWARE INITIALISATION & DEINITIALISATION
 **************************************************************************************************
 */
/*
 * @brief  Initialise the GPIO pins used to control the modem
 * @param  None
 * @retval None
 */
static void ModemGpioInit (void)
{
  gpioconfig_t PinConfig;

  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, &PinConfig);

  /*
   * Detect whether the Modem enable pin uses negative logic (HW issue A) or
   * positive logic (HW issue B+).
   * We detect by reading the un-driven state of the pin to see whether it is
   * pulled up or down.
   */
  if (IO_PIN_RESET == GpioReadPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN))
  {
    ModemEnablePositive = true;
    TRACE_MODEM("Modem enable positive (HW issue B+)\n\r");
  }
  else
  {
    ModemEnablePositive = false;
    TRACE_MODEM("Modem enable negative (HW issue A)\n\r");
  }

  /* Configure GPIO output pins : GSM_POWER_ENABLE_PIN, GSM_POWERKEY_PIN & DTR_TO_MODEM_PIN */
  PinConfig.Direction = IO_OUTPUT_PP;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, &PinConfig);
  GpioInitPin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, &PinConfig);
  GpioInitPin(DTR_TO_MODEM_PORT, DTR_TO_MODEM_PIN, &PinConfig);
  GpioInitPin(GNSS_POWER_ENABLE_PORT, GNSS_POWER_ENABLE_PIN, &PinConfig);
  if (modemSupportStandAlone)
  {
    GpioWritePin(GNSS_POWER_ENABLE_PORT, GNSS_POWER_ENABLE_PIN, IO_PIN_RESET);  /* GNSS power off */
  }

  /* Configure GPIO input pins : RI_FROM_MODEM_Pin DCD_FROM_MODEM_Pin */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(RI_FROM_MODEM_PORT, RI_FROM_MODEM_PIN, &PinConfig);
  GpioInitPin(DCD_FROM_MODEM_PORT, DCD_FROM_MODEM_PIN, &PinConfig);

  /* Configure GPIO pin power on level */
  ModemGSMPowerControl(false);   /* modem,gsm off */

  GpioWritePin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, IO_PIN_RESET);  /* PWRKEY pull high */
}


/*
 * @brief  De-initialise the GPIO pins used by the modem
 * @param  None
 * @retval None
 */
static void ModemGpioDeInit (void)
{
  GpioDeInitPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN);
  GpioDeInitPin(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN);
  GpioDeInitPin(DTR_TO_MODEM_PORT, DTR_TO_MODEM_PIN);
  GpioDeInitPin(RI_FROM_MODEM_PORT, RI_FROM_MODEM_PIN);
  GpioDeInitPin(DCD_FROM_MODEM_PORT, DCD_FROM_MODEM_PIN);
}



/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Power the modem on or off
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ModemConsolePower (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    ConsolePrintf(" Modem Power State: %s\n\r", (MPS_ALL_OFF == ModemPowerState) ? "Off": "On");
    return (CONSOLE_ERROR_OK);
  }
  if (1 == argCount)
  {
    if ((!strcmp(pArguments[0], "on")) || (!strcmp(pArguments[0], "up")))
    {
      ModemPowerUp();
    }
    if ((!strcmp(pArguments[0], "off")) || (!strcmp(pArguments[0], "down")))
    {
      ModemPowerDown();
    }
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Test power the modem on or off using PWRkey
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ModemConsoleTest (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    ConsolePrintf(" Modem Power State: %s\n\r", (MPS_ALL_OFF == ModemPowerState) ? "Off": "On");
    return (CONSOLE_ERROR_OK);
  }
  if (1 == argCount)
  {
    if ((!strcmp(pArguments[0], "on")) || (!strcmp(pArguments[0], "up")))
    {
      ModemPWRkeyControl (true);
    }
    if ((!strcmp(pArguments[0], "off")) || (!strcmp(pArguments[0], "down")))
    {
      ModemPWRkeyControl(false);
    }
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Trigger the SMS receive process
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ModemConsoleSmsRx (int argCount, char **pArguments)
{
  ModemSmsReceive();
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       upload the custom audio files to modem
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ModemConsoleFileUpload (int argCount, char **pArguments)
{
  GsmAudioBufferTransferToModem(custom_start_buf, RIP_CUSTOM_START_FILE_NAME, RIP_CUSTOM_START_FILE_SIZE);
  GsmAudioBufferTransferToModem(custom_cancel_buf, RIP_CUSTOM_CANCEL_FILE_NAME, RIP_CUSTOM_CANCEL_FILE_SIZE);
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Playback the file  thru modem in the RAM: Drive
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ModemConsoleFilePlay (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    /* send the audio file in Modem RAM*/
    GsmAudioPlayFile(pArguments[0]);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Send an SMS message
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ModemConsoleSmsTx (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    /* send the SMS message */
    ModemSmsSend(pArguments[0]);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Send and AT command to the modem
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ModemConsoleSendAtCommand (int argCount, char **pArguments)
{
  int Timeout = 0;

  if (1 == argCount)
  {
    gsmCommsSendAtCommand(pArguments[0], DEFAULT_AT_TIMEOUT_MS);
    return (CONSOLE_ERROR_OK);
  }
  else if (2 == argCount)
  {
    sscanf(pArguments[1], "%d", &Timeout);
    gsmCommsSendAtCommand(pArguments[0], Timeout);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Display the modem status
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ModemConsoleStatus (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* last VALID position */
    ConsolePrintf("GNSS INFORMATION\n\r");
    ConsolePrintf("  Lock: %s\n\r", (ModemStatus.GnssData.Status & 1) ? "Yes" : "No");
    ConsolePrintf("  Sats: %d\n\r", (int) (ModemStatus.GnssData.Status >> 3));
    ConsolePrintf("  Heading: %d\n\r", (int) ModemStatus.GnssData.Heading);
    ConsolePrintf("  Speed: %d kph\n\r", (int) ModemStatus.GnssData.Speed);
    ConsolePrintf("  Lat: %d\n\r", (int) ModemStatus.GnssData.Latitude);
    ConsolePrintf("  Long: %d\n\r", (int) ModemStatus.GnssData.Longitude);
    ConsolePrintf("  TTFF: %d s\n\r", (int) ModemStatus.GPSTimeToFix);

    uint8_t *imei_ptr = (uint8_t *)&ModemStatus.IMEI;
    /* last known cell information */
    ConsolePrintf("CELLULAR INFORMATION\n\r");

    ConsolePrintf("  IMEI: ");
    for (int i = 0; i < sizeof(ModemStatus.IMEI); i++) 
    {
      ConsolePrintf("%02X ", imei_ptr[i]);
    }
    ConsolePrintf("\r\n");
    ConsolePrintf("  Country Code %d\n\r", (int) ModemStatus.countryCode);
    ConsolePrintf("  Operator Code %d\n\r", (int) ModemStatus.operatorCode);    
    ConsolePrintf("  LAC %x\n\r", (int) ModemStatus.GsmLac);
    ConsolePrintf("  CID %x\n\r", (int) ModemStatus.GsmCid);
    ConsolePrintf("  RSSI %d\n\r", (int) ModemStatus.RSSI);
    ConsolePrintf("  Last Online time:");
    RtcPrintTimestamp(GsmGprsGetLastOnLineTime());
    ConsolePrintf("\r\n");

    /* Jamming information */
    ConsolePrintf("  Jammed: %s\n\r", ModemStatus.Jammed ? "yes" : "no");

    /* Hardware setting */
    ConsolePrintf("  Stand alone mode enabled?  %s\n\r", modemSupportStandAlone ? "yes" : "no");

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}

