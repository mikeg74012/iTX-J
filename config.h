/**
 **************************************************************************************************
 * @file   config.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   12/11/2019
 *
 * @brief  Handles Config Data
 *
 **************************************************************************************************
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "main.h"
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * Current format version for the config store.
 * Incremented each time the config structure changes
 */
#define CURRENT_CONFIG_VERSION   12

/* function return values */
#define FLASH_OPERATION_SUCCESS  0
#define FLASH_PAGE_ERROR         1
#define FLASH_WRITE_FAIL         2
#define FLASH_READ_FAIL          3
#define MAX_MOBILE_NUMBER_SIZE   18
#define MAX_MOBILE_BUF_SIZE      (MAX_MOBILE_NUMBER_SIZE + 1)
#define MAX_URL_SIZE             146
#define MAX_URL_BUF_SIZE         (MAX_URL_SIZE + 1)
#define MAX_IP_ADDRESS_SIZE      15                 /* should really be 16 to include string terminator */
#define MAX_IP_ADDRESS_BUF_SIZE  (MAX_IP_ADDRESS_SIZE + 1)
#define MAX_USERNAME_SIZE        15
#define MAX_PASSWORD_SIZE        15


/* maximum number of flags and parameters in the config store */
#define CONFIG_MAX_FLAGS         16
#define CONFIG_MAX_PARAMS        16

/* Flags identifiers - use to set / get flag values */
#define CONFIG_FLAG_CONSOLE             0
#define CONFIG_FLAG_FW_UPDATE           1
#define CONFIG_FLAG_USE_HTTP            2
#define CONFIG_FLAG_SMS_ACK             3
#define CONFIG_FLAG_SMS_DIM             4
#define CONFIG_FLAG_SEND_MEV            5
#define CONFIG_FLAG_RADIO_SCAN          6
#define CONFIG_FLAG_CUSTOM_TONE         7
#define CONFIG_FLAG_STANDBY_LP_SLEEP    8
#define CONFIG_FLAG_XYZ_NORMALISTATION  9
#define CONFIG_FLAG_GNSS_JAMMING_DETECT 10
#define CONFIG_FLAG_OTA_BL_UPDATE       11
#define CONFIG_FLAG_IMPACT_FILTER       12
#define CONFIG_SOJ_NEW_ALGO             13
#define CONFIG_FLAG_SURVEY_DATA_ENABLED 14
#define CONFIG_FLAG_LOWBAT_SHTDN        15

/* Paramter identifiers - use to set / get parameter values */
#define CONFIG_PARAM_SOJ_MAGNITUDE      0
#define CONFIG_PARAM_SOJ_DURATION       1
#define CONFIG_PARAM_EOJ_MAGNITUDE      2
#define CONFIG_PARAM_EOJ_DURATION       3
#define CONFIG_PARAM_MINIEV_MAGNITUDE   4
#define CONFIG_PARAM_MINIEV_DURATION    5
#define CONFIG_PARAM_IMPACT_MAGNITUDE   6
#define CONFIG_PARAM_IMPACT_DURATION    7
#define CONFIG_PARAM_IMPACT_SPEED       8
#define CONFIG_PARAM_MOUNT_ANGLES       9
#define CONFIG_PARAM_CORNER_MAGNITUDE   10
#define CONFIG_PARAM_CORNER_DURATION    11
#define CONFIG_PARAM_CORNER_SPEED       12
#define CONFIG_PARAM_RADIO_SCAN_SPEED   14

/*
  Usaage of CONFIG_PARAM_MOUNT_PARAM, Mount checking related settings
  Byte0: Delta value to decide device is mounted/unmounted from CONFIG_PARAM_MOUNT_ANGLES
  Byte1: Delta value Allowed to update with Surveyed data to Param CONFIG_PARAM_MOUNT_ANGLES after each survey
  Byte2: Expected Roll for this Car (set from Cloud)
  Byte3: Expected Pitch for this Car  (set from Cloud)
*/
#define CONFIG_PARAM_MOUNT_PARAM        13

/*
  Usaage of CONFIG_PARAM_MNTSURVEY_SETTING, Collection related settings
  Byte0: Data Collection Starting Speed 
  Byte1: Data Collection Durations 
  Byte2: Survey Picking Method (Peak, Average etc.)
  Byte3: Reserved 
*/
#define CONFIG_PARAM_MNTSURVEY_SETTING  15


#define CONFIG_PARAM_GET_BYTE0(param) ((param) & 0xFF)             // Extract Byte 0
#define CONFIG_PARAM_GET_BYTE1(param) (((param) >> 8) & 0xFF)      // Extract Byte 1
#define CONFIG_PARAM_GET_BYTE2(param) (((param) >> 16) & 0xFF)     // Extract Byte 2
#define CONFIG_PARAM_GET_BYTE3(param) (((param) >> 24) & 0xFF)     // Extract Byte 3

#define MM_RADIO_SCAN_SPEED_KPH          50                  // minimum vehicle speed for radio scan (speed from GNSS in unit of kph*10)


/*
 * The config data structure
 */
typedef struct
{
  /* the config store version - increments when the store changes */
  uint32_t ConfigVersion;
  uint16_t WustFlag;                                     /* Indicates whether Wake-Up Self-Test has occurred since hibernation */
  bool     TestModeNotUsed;                              /* Obsoleted - Factory test mode, this bit always set false in flash */

  /* Access Point details */
  char     ApnUrl[MAX_URL_SIZE];
  char     ApnUsername[MAX_USERNAME_SIZE];
  char     ApnPassword[MAX_PASSWORD_SIZE];

  /* communication related */
  char     VoiceCallNumber[MAX_MOBILE_BUF_SIZE];         /* Voice call number used in case of emergency event (impact or red button press) */
  char     SmsNumber[MAX_MOBILE_BUF_SIZE];               /* SMS number used to send emergency SMS data to.  Also used for debug and status SMS */
  char     GprsUrl[MAX_URL_BUF_SIZE];                    /* URL of server to connect to if HTTP(s) communications are specified */
  char     FwUpdateUrl[MAX_URL_BUF_SIZE];                /* URL of firmware update server */
  char     IpAddress[MAX_IP_ADDRESS_BUF_SIZE];           /* IP address of server to connect to if raw TCP communications are specified */
  uint32_t IpPort;                                       /* IP port of server to connect to if raw TCP communications are specified */
  uint8_t  Customer;                                     /* Customer ID */

  /* Option flags */
  bool     Flags[CONFIG_MAX_FLAGS];

  /* Settings */
  uint32_t Parameters[CONFIG_MAX_PARAMS];

  /* CRC32 checksum across entire config store.  Must be the last item! */
  uint32_t Checksum;

} configData_t;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
int      ConfigReadData             (configData_t *readConfigData);
int      ConfigWriteData            (configData_t *writeConfigData);
bool     ConfigCheckValidData       (void);
bool     ConfigCompareWithCurrent   (configData_t * pNewStore, bool includeCrc);
uint16_t ConfigReadWustFlag         (void);
bool     ConfigWriteWustFlag        (uint16_t flag);
void     ConfigHandleViaSMS         (char *configViaSMS);
bool     ConfigResetLowBatFlag      (void);
char *   ConfigGetSmsNumber         (void);
char *   ConfigGetVoiceNumber       (void);
char *   ConfigGetGprsUrl           (void);
char *   ConfigGetFwUpdateUrl       (void);
char *   ConfigGetIpAddress         (void);
uint32_t ConfigGetIpPort            (void);
char *   ConfigGetApnUrl            (void);
char *   ConfigGetApnUsername       (void);
char *   ConfigGetApnPassword       (void);
bool     ConfigGetSOJUseNewAlgo     (void);
void     ConfigSetDefaults          (void);
bool     ConfigGetFlag              (uint8_t flag);
bool     ConfigSetFlag              (uint8_t flag, bool value);
uint32_t ConfigGetParam             (uint8_t param);
bool     ConfigSetParam             (uint8_t param, uint32_t value);
uint8_t  ConfigGetCustomer          (void);
bool     ConfigSetCustomer          (uint8_t customer);

/* Console Commands */
int      ConfigConsoleDefault       (int argCount, char **pArguments);
int      ConfigConsoleSet           (int argCount, char **pArguments);
int      ConfigConsoleGet           (int argCount, char **pArguments);
int      ConfigConsoleSMS           (int argCount, char **pArguments);
int      SetHardwareControlData     (int argCount, char **pArguments);

#endif /* CONFIG_H */
