/*
 **************************************************************************************************
 * @file   modem.h
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

#ifndef MC60E_MODEM_H_
#define MC60E_MODEM_H_

#include "gsmcomms.h"
#include "gsmgnss.h"
#include "Messages.h"
#include "messagequeue.h"


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Debug trace for modem and sub-modules */
#if TRACE_MODEM_ENABLE
  #define TRACE_MODEM(...) TRACE_COLOUR_(FG_BLUE, BG_DEFAULT, "MC60E", __VA_ARGS__)
#else
  #define TRACE_MODEM(...)
#endif

/* max length of AT command that can be entered at the console */
#define MAX_CONSOLE_AT_LENGTH            100

/* Modem message types for messages posted to the modem task */
typedef enum
{
  /* Messages from the Comms Task - DEPRECATED */
  MODEM_MSG_AT_COMPLETE,
  MODEM_MSG_AT_ERROR,
  MODEM_MSG_AT_TIMEOUT,
  MODEM_MSG_AT_INFO,
  MODEM_MSG_SMS_RECEIVED,

  /* Messages from the Sequencer */
  MODEM_MSG_ABORT,
  MODEM_MSG_POWER_ON,
  MODEM_MSG_POWER_OFF,
  MODEM_MSG_POWER_OFF_GSM,
  MODEM_MSG_SELF_TEST,
  MODEM_MSG_CHK_GSM_INFO,
  MODEM_MSG_VOICE_CALL,
  MODEM_MSG_SMS_SEND,
  MODEM_MSG_SMS_RECEIVE,
  MODEM_MSG_BLE_SCAN,
  MODEM_MSG_TCP_SEND,
  MODEM_MSG_TCP_SEND_QUEUED,
  MODEM_MSG_HTTP_SEND,
  MODEM_MSG_HTTP_SEND_QUEUED,
  MODEM_MSG_HTTP_DOWNLOAD,
  MODEM_MSG_GNSS_START,
  MODEM_MSG_GNSS_STOP,
  MODEM_MSG_GNSS_READ,
  MODEM_MSG_AUDIO_CONFIG,
  MODEM_MSG_AUDIO_PLAY,
  MODEM_MSG_AUDIO_STOP,
  MODEM_MSG_JAM_SCAN_ON,
  MODEM_MSG_JAM_SCAN_OFF,
} eModemMessage;


/* Structure for messages sent to the modem task */
typedef struct
{
  uint8_t msgId;
  void *  parameter;
} ModemMessage_t;


/* current modem status information */
typedef struct
{
  uint64_t   IMEI;

  /* last VALID position */
  GnssData_t GnssData;

  /* Store GPS time to fix in seconds from start till we get a valid GPS data */
  uint16_t   GPSTimeToFix;

  /* last known cell information */
  uint16_t   GsmLac;
  uint16_t   GsmCid;
  uint16_t   RSSI;
  uint16_t   countryCode;
  uint16_t   operatorCode;

  /* Jamming information */
  bool       Jammed;

  /* TCP/IP Information */
  bool       gprsIdle;     /* The TCP/IP stack is in idle state */
  bool       gprsReg;       /* The TCP/IP stack has been registered */

} ModemStatus_t;


#define MAX_SMS_LEN               160 /* max Single SMS length */
#define GNSS_DATA_SIZE            128 /* temporary added to store all the response for read GPS command */
#define GNSS_RTC_DATA_SIZE        30
#define GSM_POWER_ON              1
#define GSM_POWER_OFF             0

/* types of tones available for play back */
typedef enum
{
  MODEM_TONE_LOW_BATTERY,
  MODEM_TONE_EMERGENCY,
  MODEM_TONE_CUSTOM_EMERGENCY_START,
  MODEM_TONE_CUSTOM_EMERGENCY_CANCEL,
} eModemTone;


/* types connection error when sending packet */
typedef enum {
  /* No error, all stages passed */
  NETWORK_NO_ERR,

  /* Network config related error */
  NETWORK_MODEM_ERR,
  NETWORK_APN_ERROR,

  /* GSM Network related error */
  NETWORK_GSM_REG_ERR,

  /* GPRS Network related error*/
  NETWORK_GPRS_REG_ERR,
  NETWORK_TCPIP_REG_ERR,
  NETWORK_PDP_ACTIVATE_ERR,             /* Activate error, not timeout (e.g. Authentication Error) */
  NETWORK_PDP_ACTIVATE_TIMEOUT,

  /* Communication errors */
  NETWORK_TCP_ERROR,            /* TCP Connection Error */
  NETWORK_HTTP_ERROR,           /* Http Connection Error */
  NETWORK_PACKETSEND_ERROR,     /* Packet sent failed (Not by ModemAbort)*/
} NetworkConnError_t;



/**
 **************************************************************************************************
 *                                         PUBLIC VARIABLES
 **************************************************************************************************
 */
/* public status information */
extern ModemStatus_t ModemStatus;




/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void      ModemTask                 (void const * argument);
bool      ModemWUST                 (void);
eAtResult ModemReceiveATResponse    (char * atResponse);
void      ModemReceiveGnssMessage   (char * pMessage);

/* Sequencer interface */
void      ModemPowerUp              (void);
void      ModemPowerDown            (void);
void      ModemPowerDownGsm         (void);
void      ModemCheckGSMInfo         (void);
void      ModemMakeVoiceCall        (char * pNumber);
void      ModemSmsSend              (char * pMessage);
void      ModemSmsReceive           (void);
void      ModemDefaultSend          (uint8_t * pData, uint32_t size);
void      ModemDefaultSendQueued    (MessagePriority_e priority);
void      ModemTcpSend              (uint8_t * pData, uint32_t size);
void      ModemTcpSendQueued        (MessagePriority_e priority);
void      ModemHttpSend             (uint8_t * pData, uint32_t size);
void      ModemHttpSendQueued       (MessagePriority_e priority);
void      ModemHttpDownload         (char * fileUrl, int maxSize, int sramAddress);
void      ModemGnssStart            (void);
void      ModemGnssStop             (void);
void      ModemGnssJamControl       (bool nMode);
void      ModemGnssGetData          (bool storeToSram);
void      ModemAudioConfigure       (void);
void      ModemAudioPlayTone        (eModemTone toneType);
void      ModemAudioPlayToneStop    (eModemTone toneType);
void      ModemBleScan              (void);
void      ModemGetRSSI              (void);
void      ModemAbort                (void);
void      ModemSupportGSMOffMode    (bool support);
bool      isModemAbort              (void);
bool      isModemSupportGSMOffMode  (void);
void      ModemMarkPowerRetainNeeded(bool retain);
void      ModemSequencerDone        (bool result);
void      ModemAbortReset           (void);
void      ModemLogNetworkError      (NetworkConnError_t error);
NetworkConnError_t ModemGetNetworkError   (void);


/* Console Commands */
int       ModemConsolePower         (int argCount, char **pArguments);
int       ModemConsoleSmsRx         (int argCount, char **pArguments);
int       ModemConsoleSmsTx         (int argCount, char **pArguments);
int       ModemConsoleFileUpload    (int argCount, char **pArguments);
int       ModemConsoleFilePlay      (int argCount, char **pArguments);
int       ModemConsoleSendAtCommand (int argCount, char **pArguments);
int       ModemConsoleStatus        (int argCount, char **pArguments);
int       ModemConsoleTest          (int argCount, char **pArguments);

#endif /* MC60E_MODEM_H_ */

