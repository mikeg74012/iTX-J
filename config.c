/**
 **************************************************************************************************
 * @file   Config.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Handles Config Data
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the Config module */
#if TRACE_CONFIG_ENABLE
  #define TRACE_CONFIG(...) TRACE_("CONFIG", __VA_ARGS__)
#else
  #define TRACE_CONFIG(...)
#endif

#define ONE_PAGE_SIZE_BYTES      2048
#define NO_OF_CONFIG_PAGES       1
#define MAX_CONFIG_DATA_BYTES    (ONE_PAGE_SIZE_BYTES * NO_OF_CONFIG_PAGES)
#define FLASH_ERASED_BYTE        0xFF


/* Default Config */
#define DEFAULT_APN_URL          "stream.co.uk"
#define DEFAULT_APN_USERNAME     "streamip"
#define DEFAULT_APN_PASSWORD     "streamip"



/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static uint32_t ConfigPageAddrs = CONFIG_ADDRESS;

/* RAM copy of the Config page */
static configData_t currentConfigData;

/* Default values to be used on new units */
static configData_t ConfigDefaults =
{
  /* Header info */
  CURRENT_CONFIG_VERSION,     // ConfigVersion
  WUST_NOT_DONE,              // WustFlag
  false,                      // Obsoleted - Factory test mode, this bit always set false in flash

  /* Access Point details */
  "stream.co.uk",               // ApnUrl[MAX_URL_SIZE];
  "streamip",                   // ApnUsername[MAX_USERNAME_SIZE];
  "streamip",                   // ApnPassword[MAX_PASSWORD_SIZE];

  /* communication related */
  "",                           // VoiceCallNumber[MAX_MOBILE_BUF_SIZE];
  "",                           // SmsNumber[MAX_MOBILE_BUF_SIZE];
  "https://itelematixlistenerv4.azurewebsites.net/api/Listener",  // GprsUrl[MAX_URL_BUF_SIZE];
  "https://syno.ripcorddesigns.com/client/itelematix",            // FwUpdateUrl[MAX_URL_BUF_SIZE];
  "20.105.151.36",              // IpAddress[MAX_IP_ADDRESS_BUF_SIZE];
  9010,                         // IpPort;
  0,                            // Customer ID

  /* Option Flags */
  {
   false,                       // Flag 0 - CONFIG_FLAG_CONSOLE
   false,                       // Flag 1 - CONFIG_FLAG_FW_UPDATE
   false,                       // Flag 2 - CONFIG_FLAG_USE_HTTP
   true,                        // Flag 3 - CONFIG_FLAG_SMS_ACK
   false,                       // Flag 4 - CONFIG_FLAG_SMS_DIM
   true,                        // Flag 5 - CONFIG_FLAG_SEND_MEV
   false,                       // Flag 6 - CONFIG_FLAG_RADIO_SCAN
   true,                        // Flag 7 - CONFIG_FLAG_CUSTOM_TONE
   true,                        // Flag 8 - CONFIG_FLAG_STANDBY_LP_SLEEP
   false,                       // Flag 9 - CONFIG_FLAG_XYZ_NORMALISTATION
   true,                        // Flag 10 - CONFIG_FLAG_GNSS_JAMMING_DETECT
   false,                       // Flag 11 - CONFIG_FLAG_OTA_BL_UPDATE
   false,                       // Flag 12 - CONFIG_FLAG_IMPACT_FILTER
   false,                       // Flag 13 - CONFIG_SOJ_NEW_ALGO
   false,                       // Flag 14 - NOT CURRENTLY USED
   false,                       // Flag 15 - CONFIG_FLAG_LOWBAT_SHTDN
  },

  /* Parameter Values */
  {
    SOJ_DETECT_THRESHOLD_XYZ,     // Parameter 0 - Minimum magnitude for SOJ detect
    SOJ_DETECT_TIMEOUT_MS,        // Parameter 1 - Minimum duration for SOJ detect
    EOJ_DETECT_THRESHOLD_XYZ,     // Parameter 2 - Minimum magnitude for EOJ detect
    EOJ_DETECT_TIMEOUT_MS,        // Parameter 3 - Minimum duration for EOJ detect
    MM_MINIEV_DETECT_THRESHOLD,   // Parameter 4 - Minimum magnitude for mini-event detect
    MM_MINIEV_MIN_DURATION_MS,    // Parameter 5 - Minimum duration for mini-event detect (NOT CURRENTLY USED)
    MM_IMPACT_DETECT_THRESHOLD,   // Parameter 6 - Minimum magnitude for impact detect
    MM_IMPACT_MIN_DURATION_MS,    // Parameter 7 - Minimum duration for impact detect
    MM_IMPACT_MIN_SPEED_KPH,      // Parameter 8 - Minimum speed for impact detect
    MOUNT_ANGLE_DEFAULT,          // Parameter 9 - Mounting pitch and roll angle
    MM_CORNER_DETECT_THRESHOLD_Y, // Parameter 10 - Minimum magnitude for Corner detect
    MM_CORNER_MIN_DURATION_MS,    // Parameter 11 - Minimum duration for Corner detect
    MM_CORNER_MIN_SPEED_KPH,      // Parameter 12 - Minimum speed for Corner detect
    MOUNT_PARAM_DEFAULT,          // Parameter 13 - Mounting related Param, 4bytes usage
    MM_RADIO_SCAN_SPEED_KPH,      // Parameter 14 - Default speed that will allow Radio scan
    MNTSURVEY_SETTING_DEFAULT,    // Parameter 15 - Mount Survey Collection related settings, 4bytes usage
  },

  /* CRC32 checksum across entire Config store.  Must be the last item! */
  0                               // Checksum;
};



/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void     ConfigErasePage            (void);
static bool     ConfigWriteFlash           (configData_t * pWriteBuffer, int sizeInBytes);
static void     ConfigReadFlash            (uint32_t index, configData_t *readBuffer, int sizeInByteWriteStruct);
static bool     ConfigWriteCustomer        (uint8_t customer);
static bool     ConfigWriteVoiceCallNumber (char *number);
static bool     ConfigWriteSMSNumber       (char *number);
static bool     ConfigWriteGprsUrl         (char * url);
static bool     ConfigWriteFwUpdateUrl     (char * url);
static bool     ConfigWriteIpAddress       (char * ipAddress);
static bool     ConfigWriteIpPort          (uint32_t port);
static bool     ConfigWriteApnUrl          (char * url);
static bool     ConfigWriteApnUsername     (char * url);
static bool     ConfigWriteApnPassword     (char * url);
static uint32_t ConfigGenerateCrc          (configData_t *pConfigData);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Write the Config Data
 * @param     writeConfigData - struct which contains the Config data to be written into flash
 * @return    returns any one of the error code:
 *            FLASH_OPERATION_SUCCESS  0
 *            FLASH_PAGE_ERROR         1
 *            FLASH_WRITE_FAIL         2
 *            FLASH_READ_FAIL          3
 */
int ConfigWriteData (configData_t *writeConfigData)
{
  int sizeOfstruct = sizeof(configData_t);

  /*
   * Check if the Data to be written is greater than one page defined for Config
   * If required no. of Config page can be changed, which is at the moment set to one
   */
  if (sizeOfstruct > MAX_CONFIG_DATA_BYTES)
  {
    TRACE_CONFIG("Config write - Data greater than one page size\n\r");
    return FLASH_PAGE_ERROR;
  }

  /* Generate the CRC */
  writeConfigData->Checksum = ConfigGenerateCrc(writeConfigData);

  /* Write to Internal Flash Memory */
  if (ConfigWriteFlash(writeConfigData, sizeOfstruct))
  {
    TRACE_CONFIG("Config write - Success\n\r");
    return FLASH_OPERATION_SUCCESS;
  }

  TRACE_CONFIG("Config write - Failed\n\r");
  return FLASH_WRITE_FAIL;
}


/**
 * @brief     Read the Config Data from FLASH into RAM
 * @param     readConfigData - struct to store the Config Data
 * @return    returns any one of the error code:
 *            FLASH_OPERATION_SUCCESS  0
 *            FLASH_PAGE_ERROR         1
 *            FLASH_WRITE_FAIL         2
 *            FLASH_READ_FAIL          3
 * @warning   at the moment FLASH_OPERATION_SUCCESS on each time
 **/
int ConfigReadData  (configData_t *readConfigData)
{
  ConfigReadFlash(0, readConfigData, sizeof(configData_t));
  return FLASH_OPERATION_SUCCESS;
}


/**
 * @brief     Checks if flash contains valid Config data
 * @param     None
 * @return    true: if it contains some valid data
 *            false: It does not contain any valid data
 */
bool ConfigCheckValidData (void)
{
  bool result = true;
  configData_t *pMem = NULL;

  /* point to the data store */
  pMem = (configData_t*)ConfigPageAddrs;

  /* Check for correct version of store */
  if (pMem->ConfigVersion != CURRENT_CONFIG_VERSION)
  {
    result = false;
  }

  /* Validate the checksum */
  if (result)
  {
    if (pMem->Checksum != ConfigGenerateCrc(pMem))
    {
      result = false;
    }
  }

  return result;
}


/**
 * @brief     Read Wake-Up Self-Test flag from Config data
 * @param     none
 * @return    The flag
 */
uint16_t ConfigReadWustFlag (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  return pMem->WustFlag;
}


/**
 * @brief     Reset the Low Battery Flag in flash
 * @param     none
 * @return    bool: true on success, false on failure
 */
bool ConfigResetLowBatFlag (void)
{
  return ConfigSetFlag(CONFIG_FLAG_LOWBAT_SHTDN, false);
}


/**
 * @brief     Handle Config Via SMS
 * @param     Config Data via SMS
 * @return    None
 */
void ConfigHandleViaSMS (char *ConfigViaSMS)
{
  TRACE_CONFIG("Config Received: %s\n\r",ConfigViaSMS);
  ConfigUpdateConfigFromSms(ConfigViaSMS);
}


/**
 * @brief     Get the voice call number from the config store
 * @param     None
 * @return    pointer to voice call number from the memory
 */
char * ConfigGetVoiceNumber (void)
{
  configData_t * pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_MOBILE_BUF_SIZE == strnlen(pMem->VoiceCallNumber, MAX_MOBILE_BUF_SIZE))
  {
    return "";
  }
  return pMem->VoiceCallNumber;
}


/**
 * @brief     Get the SMS number from the config store
 * @param     None
 * @return    pointer to smsNum from the memory
 */
char * ConfigGetSmsNumber (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_MOBILE_BUF_SIZE == strnlen(pMem->SmsNumber, MAX_MOBILE_BUF_SIZE))
  {
    return "";
  }
  return pMem->SmsNumber;
}


/**
 * @brief     Get the GPRS URL from the config store
 * @param     None
 * @return    pointer to GPRS URL in FLASH
 */
char * ConfigGetGprsUrl (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_URL_BUF_SIZE == strnlen(pMem->GprsUrl, MAX_URL_BUF_SIZE))
  {
    return "";
  }
  return pMem->GprsUrl;
}


/**
 * @brief     Get the FW Update URL from the config store
 * @param     None
 * @return    pointer to FW Update URL in FLASH
 */
char * ConfigGetFwUpdateUrl (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_URL_BUF_SIZE == strnlen(pMem->FwUpdateUrl, MAX_URL_BUF_SIZE))
  {
    return "";
  }
  return pMem->FwUpdateUrl;
}


/**
 * @brief     Get the IP address from the config store
 * @param     None
 * @return    pointer to IP address in FLASH
 */
char * ConfigGetIpAddress (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_IP_ADDRESS_BUF_SIZE == strnlen(pMem->IpAddress, MAX_IP_ADDRESS_BUF_SIZE))
  {
    return "";
  }
  return pMem->IpAddress;
}


/**
 * @brief     Get the IP port number from the config store
 * @param     None
 * @return    The IP port number
 */
uint32_t ConfigGetIpPort (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  return pMem->IpPort;
}


/**
 * @brief     Write Wake-Up Self-Test flag tocConfig data
 * @param     The flag
 * @return    Whether the write was successful
 */
bool ConfigWriteWustFlag (uint16_t flag)
{
  bool success = false;

  /* Read the Config page from FLASH */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update the flag and write Config page back to FLASH */
    currentConfigData.WustFlag = flag;
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/*
 * @brief     Get the APN URL from the config store
 * @param     None
 * @return    pointer to APN URL
 */
char *   ConfigGetApnUrl (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_URL_SIZE == strnlen(pMem->ApnUrl, MAX_URL_SIZE))
  {
    return "";
  }
  return pMem->ApnUrl;
}


/*
 * @brief     Get the APN Username from the config store
 * @param     None
 * @return    pointer to APN Username
 */
char * ConfigGetApnUsername (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_USERNAME_SIZE == strnlen(pMem->ApnUsername, MAX_USERNAME_SIZE))
  {
    return "";
  }
  return pMem->ApnUsername;
}


/*
 * @brief     Get the APN Password from the config store
 * @param     None
 * @return    pointer to APN Password
 */
char * ConfigGetApnPassword (void)
{
  configData_t *pMem = NULL;
  pMem = (configData_t*)ConfigPageAddrs;
  if (MAX_PASSWORD_SIZE == strnlen(pMem->ApnPassword, MAX_PASSWORD_SIZE))
  {
    return "";
  }
  return pMem->ApnPassword;
}


/*
 * @brief     Get the Customer ID from the config store
 * @param     None
 * @return    The customer number
 */
uint8_t ConfigGetCustomer (void)
{
  configData_t *pMem = NULL;
  uint8_t Customer;

  if (true == TestModeGet())
  {
    Customer = 255;
  }
  else
  {
    pMem = (configData_t*)ConfigPageAddrs;
    Customer = pMem->Customer;
  }
  return Customer;
}

/**
 * @brief     Get if SOJ should use the new Algo or not in settings
 * @param     None
 * @return    true if New SOJ should use, otherwise false
 */
bool ConfigGetSOJUseNewAlgo(void)
{
  bool result = false;

  configData_t *pMem = (configData_t*)ConfigPageAddrs;
  result = (pMem->Flags[CONFIG_SOJ_NEW_ALGO] == 1);

  return result;

}

/**
 * @brief     Set the default config values
 * @warning   The caller is responsible for checking if defaults should be set before calling this function
 * @param     None
 * @return    None
 */
void ConfigSetDefaults (void)
{
  configData_t setDefaultConfigData;

  TRACE_CONFIG("Updating to v%d\n\r", CURRENT_CONFIG_VERSION);

  /* copy the default values */
  memcpy(&setDefaultConfigData, &ConfigDefaults, sizeof(configData_t));

  /*
   * Copy some relevant fields over from the old store (currentConfigData) to the new (setDefaultConfigData).
   * In future, check CRC of old store first (store v4 does not have CRC)
   */
  ConfigReadData(&currentConfigData);
  if (4 == currentConfigData.ConfigVersion)
  {
    configv4_t *pOldConfig = (configv4_t*)ConfigPageAddrs;
    memcpy(setDefaultConfigData.SmsNumber,       pOldConfig->SmsNumber,       MAX_MOBILE_BUF_SIZE);
    memcpy(setDefaultConfigData.VoiceCallNumber, pOldConfig->VoiceCallNumber, MAX_MOBILE_BUF_SIZE);
  }
  if (5 == currentConfigData.ConfigVersion)
  {
    configv5_t *pOldConfig = (configv5_t*)ConfigPageAddrs;
    memcpy(setDefaultConfigData.SmsNumber,       pOldConfig->SmsNumber,       MAX_MOBILE_BUF_SIZE);
    memcpy(setDefaultConfigData.VoiceCallNumber, pOldConfig->VoiceCallNumber, MAX_MOBILE_BUF_SIZE);
  }
  if (6 == currentConfigData.ConfigVersion)
  {
    configv6_t *pOldConfig = (configv6_t*)ConfigPageAddrs;
    memcpy(setDefaultConfigData.SmsNumber,       pOldConfig->SmsNumber,       MAX_MOBILE_BUF_SIZE);
    memcpy(setDefaultConfigData.VoiceCallNumber, pOldConfig->VoiceCallNumber, MAX_MOBILE_BUF_SIZE);
  }

  if (7 == currentConfigData.ConfigVersion)
  {
    configv7_t *pOldConfig = (configv7_t*)ConfigPageAddrs;
    memcpy(setDefaultConfigData.SmsNumber,       pOldConfig->SmsNumber,       MAX_MOBILE_BUF_SIZE);
    memcpy(setDefaultConfigData.VoiceCallNumber, pOldConfig->VoiceCallNumber, MAX_MOBILE_BUF_SIZE);
    memcpy(setDefaultConfigData.GprsUrl,         pOldConfig->GprsUrl,         MAX_URL_BUF_SIZE);
    memcpy(setDefaultConfigData.FwUpdateUrl,     pOldConfig->FwUpdateUrl,     MAX_URL_BUF_SIZE);
    memcpy(setDefaultConfigData.IpAddress,       pOldConfig->IpAddress,       MAX_IP_ADDRESS_BUF_SIZE);
    setDefaultConfigData.IpPort   = pOldConfig->IpPort;
    setDefaultConfigData.Customer = pOldConfig->Customer;
    for (int i = 0; i < CONFIG_MAX_FLAGS_V7; i++)
    {
      setDefaultConfigData.Flags[i] = pOldConfig->Flags[i];
    }
  }

  if (8 == currentConfigData.ConfigVersion)
  {
    /* Copy everything from the old store, and only update 2 accelerometer values */
    configData_t *pOldConfig = (configData_t *)ConfigPageAddrs;
    memcpy(&setDefaultConfigData, pOldConfig, sizeof(configData_t));

    /* update the SoJ and EoJ values only */
    setDefaultConfigData.ConfigVersion = CURRENT_CONFIG_VERSION;
    setDefaultConfigData.Parameters[CONFIG_PARAM_SOJ_MAGNITUDE] = SOJ_DETECT_THRESHOLD_XYZ;
    setDefaultConfigData.Parameters[CONFIG_PARAM_EOJ_MAGNITUDE] = EOJ_DETECT_THRESHOLD_XYZ;
  }

  if (9 == currentConfigData.ConfigVersion)
  {
    /* Copy everything from the old store, and only update 2 accelerometer values */
    configData_t *pOldConfig = (configData_t *)ConfigPageAddrs;
    memcpy(&setDefaultConfigData, pOldConfig, sizeof(configData_t));

    /* update newly touched flags and parameter */
    setDefaultConfigData.ConfigVersion                          = CURRENT_CONFIG_VERSION;
    setDefaultConfigData.Flags[CONFIG_FLAG_CUSTOM_TONE]         = 0;
    setDefaultConfigData.Flags[CONFIG_FLAG_STANDBY_LP_SLEEP]    = 0;
    setDefaultConfigData.Flags[CONFIG_FLAG_XYZ_NORMALISTATION]  = 0;
    setDefaultConfigData.Flags[CONFIG_FLAG_GNSS_JAMMING_DETECT] = 0;
    setDefaultConfigData.Flags[CONFIG_FLAG_OTA_BL_UPDATE]       = 0;

    setDefaultConfigData.Parameters[CONFIG_PARAM_MOUNT_ANGLES] = MOUNT_ANGLE_DEFAULT;
    setDefaultConfigData.Parameters[CONFIG_PARAM_CORNER_MAGNITUDE]      = MM_CORNER_DETECT_THRESHOLD_Y;
    setDefaultConfigData.Parameters[CONFIG_PARAM_CORNER_DURATION]       = MM_CORNER_MIN_DURATION_MS;
    setDefaultConfigData.Parameters[CONFIG_PARAM_CORNER_SPEED]          = MM_CORNER_MIN_SPEED_KPH;
    setDefaultConfigData.Parameters[CONFIG_PARAM_MOUNT_PARAM] = MOUNT_PARAM_DEFAULT;
  }

  if (10 == currentConfigData.ConfigVersion)
  {
    /* Copy everything from the old store, and only impact filter values */
    configData_t *pOldConfig = (configData_t *)ConfigPageAddrs;
    memcpy(&setDefaultConfigData, pOldConfig, sizeof(configData_t));

    /* update newly touched flags and parameter */
    setDefaultConfigData.ConfigVersion                          = CURRENT_CONFIG_VERSION;
    setDefaultConfigData.Flags[CONFIG_FLAG_IMPACT_FILTER]       = 0;   /* no filter on impact calls */
  }

  if (11 == currentConfigData.ConfigVersion)
  {
    /* Copy everything from the old store, and update the radio scan speed (its100) */
    configData_t *pOldConfig = (configData_t *)ConfigPageAddrs;
    memcpy(&setDefaultConfigData, pOldConfig, sizeof(configData_t));

    /* update newly touched flags and parameter */
    setDefaultConfigData.ConfigVersion                                   = CURRENT_CONFIG_VERSION;
    setDefaultConfigData.Flags[CONFIG_SOJ_NEW_ALGO]                      = 0;
    setDefaultConfigData.Flags[CONFIG_FLAG_IMPACT_FILTER]                = 0;                         /* default disable the Impact call filter */
    setDefaultConfigData.Flags[CONFIG_FLAG_STANDBY_LP_SLEEP]             = true;                      /* default enable Low power standby */
    setDefaultConfigData.Flags[CONFIG_FLAG_GNSS_JAMMING_DETECT]          = true;                      /* default enable the Jam Detection */
    setDefaultConfigData.Flags[CONFIG_FLAG_CUSTOM_TONE]                  = true;                      /* default enable the Emergency Voice Prompt */

    setDefaultConfigData.Parameters[CONFIG_PARAM_IMPACT_SPEED]           = MM_IMPACT_MIN_SPEED_KPH;   /* update of the impact speed value */
    setDefaultConfigData.Parameters[CONFIG_PARAM_RADIO_SCAN_SPEED]       = MM_RADIO_SCAN_SPEED_KPH;   /* default speed for the radio scan */
    setDefaultConfigData.Flags[CONFIG_FLAG_SURVEY_DATA_ENABLED]          = 0;
    setDefaultConfigData.Parameters[CONFIG_PARAM_MNTSURVEY_SETTING]      = MNTSURVEY_SETTING_DEFAULT;
  }

  /* write to FLASH */
  ConfigWriteData(&setDefaultConfigData);
}


/**
 * @brief     Get the current value of a flag from the config store
 * @param     flag - the flag to get (see definitions in Config.h)
 * @return    value - the current flag value (true or false)
 */
bool ConfigGetFlag (uint8_t flag)
{
  bool result = false;

  if (flag < CONFIG_MAX_FLAGS)
  {
    configData_t *pMem = (configData_t*)ConfigPageAddrs;
    result = (pMem->Flags[flag] == 1);
  }

  return result;
}


/**
 * @brief     Set the value of a flag and write to the config store
 * @param     flag - the flag to get (see definitions in Config.h)
 * @param     value - the new flag value (true or false)
 * @return    true on success, false otherwise
 */
bool ConfigSetFlag (uint8_t flag, bool value)
{
  bool success = false;

  if (flag < CONFIG_MAX_FLAGS)
  {
    /* Read the Config page from FLASH */
    if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      /* Update the flag and write Config page back to FLASH */
      currentConfigData.Flags[flag] = value;
      if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
      {
        success = true;
      }
    }
  }

  if (flag == CONFIG_FLAG_CONSOLE)
  {
    /* update the printable flag */
    ConsoleUpdatePrintable();
  }
  else if (flag == CONFIG_FLAG_XYZ_NORMALISTATION)
  {
    /* update the XYZ normalisation flag */
    FitmentNormalisationEnable(value);
  }
  else if (flag == CONFIG_SOJ_NEW_ALGO)
  {
    /* Reinit the Accelator wakeup config  */
    AccelDriverWakeupThresholdConfig(); 
  }
  return success;
}


/**
 * @brief     Get the current value of a parameter from the config store
 * @param     param - the parameter to get (see definitions in Config.h)
 * @return    value - the current parameter value (32-bit)
 */
uint32_t ConfigGetParam (uint8_t param)
{
  uint32_t result = 0;

  if (param < CONFIG_MAX_PARAMS)
  {
    configData_t *pMem = (configData_t*)ConfigPageAddrs;
    result = pMem->Parameters[param];
  }

  return result;
}


/**
 * @brief     Set the value of a parameter and write to the config store
 * @param     param - the parameter to get (see definitions in config.h)
 * @param     value - the new parameter value (32-bit)
 * @return    true on success, false otherwise
 */
bool ConfigSetParam (uint8_t param, uint32_t value)
{
  bool success = false;

  if (param < CONFIG_MAX_PARAMS)
  {
    /* Read the Config page from FLASH */
    if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      /* Update the parameter and write Config page back to FLASH */
      currentConfigData.Parameters[param] = value;
      if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
      {
        success = true;
      }
    }
  }
  return success;
}


/**
 * @brief     Set the value of the customer field.
 * @param     customer - the new customer value (0 to 255)
 * @return    true on success, false otherwise
 */
bool ConfigSetCustomer (uint8_t customer)
{
  if (customer == ConfigGetCustomer())
  {
      /* no need write to flash if it is the same */
      return true;
  }
  ConsolePrintf("Config write called ! [0x%X] \r\n", GetLPSleepStatus());

  return ConfigWriteCustomer(customer);
}


/**
 * @brief     Compare a config store to the current settings
 * @param     pNewStore - the new store to compare
 * @param     includeCrc - true = include CRC, false = dont include CRC
 * @return    true if equal, false otherwise
 */
bool ConfigCompareWithCurrent (configData_t * pNewStore, bool includeCrc)
{
  int Comp;

  configData_t *pMem = (configData_t*)ConfigPageAddrs;

  if (includeCrc)
  {
    Comp = memcmp(pNewStore, pMem, sizeof(configData_t));
  }
  else
  {
    Comp = memcmp(pNewStore, pMem, sizeof(configData_t) - sizeof(uint32_t));
  }
  return (0 == Comp);
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     Generate a checksum from the config data
 * @param     ConfigData - struct to generate CRC for
 * @return    CRC - the checksum
 */
uint32_t ConfigGenerateCrc (configData_t *pConfigData)
{
  /* We generate the CRC across the entire structure except for the CRC field */
  uint32_t DataLen = sizeof(configData_t) - sizeof(pConfigData->Checksum);
  uint32_t Crc = CrcDriverCalculateCrc32((uint8_t *) pConfigData, DataLen);

  return Crc;
}


/**
 * @brief     Write the Data into the flash
 * @param     writeBuffer : struct pointer to write the latest Config data
 *            size: amount of data to be written into flash
 * @return    True: when Flash write was successfully completed
 *            false: error occurred during performing flash write
 */
static bool ConfigWriteFlash (configData_t * pWriteBuffer, int sizeInBytes)
{
  bool retval = true;

  /*
   * We cannot have another task writing or reading the flash until this function completes
   * so we enter a critical section here.  This stops schedule AND interrupts
   */
  if (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
  {
    taskENTER_CRITICAL();
  }

  /* Erase the FLASH page to be re-written */
  ConfigErasePage();

  /* Write the new config store to FLASH */
  FlashWrite(ConfigPageAddrs, (uint8_t *) pWriteBuffer, sizeInBytes);


  /* Resume interrupts and scheduler */
  if (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
  {
    taskEXIT_CRITICAL();
  }

  return retval;
}


/**
 * @brief     Reads the Data from the flash
 * @param     index: position of the Data to be read
 *            readBuffer: Structure to store latest Config Data
 *            size: amount of data to be read from flash
 * @return    None
 */
static void ConfigReadFlash (uint32_t index, configData_t *readBuffer, int size)
{
  memcpy(readBuffer, (uint16_t *)ConfigPageAddrs, size);
}


/**
 * @brief     Erase the flash
 * @param     None
 * @return    None
 */
static void ConfigErasePage (void)
{
  FlashErase(ConfigPageAddrs, NO_OF_CONFIG_PAGES);
}


/**
 * @brief     Write the customer to Config data
 * @param     the new customer
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteCustomer (uint8_t customer)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update customer and write page back */
    currentConfigData.Customer = customer;
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write voice call number to Config data
 * @param     the number to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteVoiceCallNumber (char *number)
{
  bool success = false;
  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update Voice call number and make sure it is terminated */
    strncpy(currentConfigData.VoiceCallNumber, number, MAX_MOBILE_NUMBER_SIZE);
    currentConfigData.VoiceCallNumber[MAX_MOBILE_NUMBER_SIZE] = 0;

    /* Write Config page back to FLASH */
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write SMS call number to Config data
 * @param     the number to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteSMSNumber (char *number)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update SMS number and write page back */
    strncpy(currentConfigData.SmsNumber, number, MAX_MOBILE_NUMBER_SIZE);
    currentConfigData.SmsNumber[MAX_MOBILE_NUMBER_SIZE] = 0;

    /* Write Config page back to FLASH */
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write data URL to Config data
 * @param     the url to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteGprsUrl (char * url)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update URL and make sure it is terminated */
    strncpy(currentConfigData.GprsUrl, url, MAX_URL_SIZE);
    currentConfigData.GprsUrl[MAX_URL_SIZE] = 0;

    /* Write Config page back to FLASH */
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write firmware update URL to Config data
 * @param     the url to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteFwUpdateUrl (char * url)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update URL number and make sure it is terminated */
    strncpy(currentConfigData.FwUpdateUrl, url, MAX_URL_SIZE);
    currentConfigData.FwUpdateUrl[MAX_URL_SIZE] = 0;

    /* Write Config page back to FLASH */
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write the IP address to Config data
 * @param     the new IP address
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteIpAddress (char * ipAddress)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Copy IP address and make sure it is terminated */
    strncpy(currentConfigData.IpAddress, ipAddress, MAX_IP_ADDRESS_SIZE);
    currentConfigData.IpAddress[MAX_IP_ADDRESS_SIZE] = 0;

    /* Write Config page back to FLASH */
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write the IP port to Config data
 * @param     the new IP port
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteIpPort (uint32_t port)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update port number and write page back */
    currentConfigData.IpPort = port;
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write Apn URL to Config data
 * @param     the url to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteApnUrl (char * url)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update URL number and write page back */
    strncpy(currentConfigData.ApnUrl, url, MAX_URL_SIZE);
    currentConfigData.ApnUrl[MAX_URL_SIZE-1] = '\0';
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write Apn Username to Config data
 * @param     the username to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteApnUsername (char * username)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update URL number and write page back */
    strncpy(currentConfigData.ApnUsername, username, MAX_USERNAME_SIZE);
    currentConfigData.ApnUsername[MAX_USERNAME_SIZE-1] = '\0';
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Write Apn Password to Config data
 * @param     the password to be updated
 * @return    true: write was success
 *            false: write was not success
 */
static bool ConfigWriteApnPassword (char * password)
{
  bool success = false;

  /* First must read entire Config page from Flash. */
  if (ConfigReadData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
  {
    /* Update URL number and write page back */
    strncpy(currentConfigData.ApnPassword, password, MAX_PASSWORD_SIZE);
    currentConfigData.ApnPassword[MAX_PASSWORD_SIZE-1] = '\0';
    if (ConfigWriteData(&currentConfigData) == FLASH_OPERATION_SUCCESS)
    {
      success = true;
    }
  }
  return success;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Reset all Config data to default values
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ConfigConsoleDefault (int argCount, char **pArguments)
{
  ConfigSetDefaults();
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Set a Configuration value
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ConfigConsoleSet (int argCount, char **pArguments)
{
  uint8_t processedFlag = 0;
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "apnuser"))
    {
      ConfigWriteApnUsername("");
      ConsolePrintf("apnuser reset as empty: [%s]\n\r", ConfigGetApnUsername());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "apnpass"))
    {
      ConfigWriteApnPassword("");
      ConsolePrintf("apnpass reset as empty: [%s]\n\r", ConfigGetApnPassword());
      processedFlag = 1;
    }
  }
  if (2 == argCount)
  {
    processedFlag = 2;
    if (!strcmp(pArguments[0], "testmode"))           /* DEPRECATED */
    {
      int FlagValue = 0;
      sscanf(pArguments[1], "%d", &FlagValue);
      if (FlagValue)
      {
        TestModeSet();
      }
      else
      {
        TestModeClear();
      }
      ConsolePrintf("testmode: %d\n\r", TestModeGet());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "customer"))
    {
      int Customer = 0;
      sscanf(pArguments[1], "%d", &Customer);
      ConfigWriteCustomer((uint8_t)Customer);
      ConsolePrintf("customer: %d\n\r", (int)ConfigGetCustomer());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "apnurl"))
    {
      ConfigWriteApnUrl(pArguments[1]);
      ConsolePrintf("apnurl: %s\n\r", ConfigGetApnUrl());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "apnuser"))
    {
      ConfigWriteApnUsername(pArguments[1]);
      ConsolePrintf("apnuser: %s\n\r", ConfigGetApnUsername());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "apnpass"))
    {
      ConfigWriteApnPassword(pArguments[1]);
      ConsolePrintf("apnpass: %s\n\r", ConfigGetApnPassword());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "smsnum"))
    {
      ConfigWriteSMSNumber(pArguments[1]);
      ConsolePrintf("smsnum: %s\n\r", ConfigGetSmsNumber());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "voicenum"))
    {
      ConfigWriteVoiceCallNumber(pArguments[1]);
      ConsolePrintf("voicenum: %s\n\r", ConfigGetVoiceNumber());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "gprsurl"))
    {
      ConfigWriteGprsUrl(pArguments[1]);
      ConsolePrintf("gprsurl: %s\n\r", ConfigGetGprsUrl());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "fwurl"))
    {
      ConfigWriteFwUpdateUrl(pArguments[1]);
      ConsolePrintf("fwurl: %s\n\r", ConfigGetFwUpdateUrl());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "ipaddress"))
    {
      ConfigWriteIpAddress(pArguments[1]);
      ConsolePrintf("ipaddress: %s\n\r", ConfigGetIpAddress());
      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "ipport"))
    {
      int IpPort = 0;
      sscanf(pArguments[1], "%d", &IpPort);
      ConfigWriteIpPort((uint32_t)IpPort);
      ConsolePrintf("ipport: %d\n\r",    ConfigGetIpPort());
      processedFlag = 1;
    }
  }
  if (3 == argCount)
  {
    processedFlag = 3;
    if (!strcmp(pArguments[0], "flag"))
    {
      int FlagId = 0;
      int FlagValue = 0;
      sscanf(pArguments[1], "%d", &FlagId);
      sscanf(pArguments[2], "%d", &FlagValue);
      if (ConfigSetFlag((uint8_t) FlagId, (bool)FlagValue))
      {
        ConsolePrintf("flag %d: %d\n\r", FlagId, ConfigGetFlag(FlagId));
      }

      processedFlag = 1;
    }
    if (!strcmp(pArguments[0], "param"))
    {
      int ParamId = 0;
      int ParamValue = 0;
      sscanf(pArguments[1], "%d", &ParamId);
      sscanf(pArguments[2], "%d", &ParamValue);
      if (ConfigSetParam((uint8_t) ParamId, (uint32_t)ParamValue))
      {
        ConsolePrintf("param %d: %d\n\r", ParamId, ConfigGetParam(ParamId));
      }
      processedFlag = 1;
    }
  }

  if (processedFlag == 1)  /*processed cmd */
  {
    return( CONSOLE_ERROR_OK );
  }
  else if ( (processedFlag == 2) && ((!strcmp(pArguments[0], "flag")) || (!strcmp(pArguments[0], "param"))) )
  {
    return(CONSOLE_ARG_NO);
  }
  else if (processedFlag >= 2)
  {
    ConsolePrintf("Usage: set <Config item> <value>\n\r");
    return(CONSOLE_CMD_UNKNOWN);
  }

  ConsolePrintf("Usage: set <Config item> <value>\n\r");
  ConsolePrintf("Usage: set flag n <value>\n\r");
  ConsolePrintf("Usage: set param n <value>\n\r");
  ConsolePrintf("       <Config item> can be: smsnum, voicenum, gprsurl, fwurl\n\r");
  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Display a Configuration value
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ConfigConsoleGet (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    if ((!strcmp(pArguments[0], "testmode")) || (!strcmp(pArguments[0], "all")))           /* DEPRECATED */
    {
      ConsolePrintf("testmode: %d\n\r", (int)TestModeGet());
    }
    if ((!strcmp(pArguments[0], "customer")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("customer: %d\n\r", (int)ConfigGetCustomer());
    }
    if ((!strcmp(pArguments[0], "apnurl")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("apnurl: %s\n\r", ConfigGetApnUrl());
    }
    if ((!strcmp(pArguments[0], "apnuser")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("apnuser: %s\n\r", ConfigGetApnUsername());
    }
    if ((!strcmp(pArguments[0], "apnpass")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("apnpass: %s\n\r", ConfigGetApnPassword());
    }
    if ((!strcmp(pArguments[0], "smsnum")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("smsnum: %s\n\r", ConfigGetSmsNumber());
    }
    if ((!strcmp(pArguments[0], "voicenum")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("voicenum: %s\n\r", ConfigGetVoiceNumber());
    }
    if ((!strcmp(pArguments[0], "gprsurl")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("gprsurl: %s\n\r", ConfigGetGprsUrl());
    }
    if ((!strcmp(pArguments[0], "fwurl")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("fwurl: %s\n\r", ConfigGetFwUpdateUrl());
    }
    if ((!strcmp(pArguments[0], "ipaddress")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("ipaddress: %s\n\r", ConfigGetIpAddress());
    }
    if ((!strcmp(pArguments[0], "ipport")) || (!strcmp(pArguments[0], "all")))
    {
      ConsolePrintf("ipport: %d\n\r",    ConfigGetIpPort());
    }
    if ((!strcmp(pArguments[0], "flag")) || (!strcmp(pArguments[0], "all")))
    {
      for (int i = 0; i < CONFIG_MAX_FLAGS; i++)
      {
        ConsolePrintf("flag %d: %d\n\r", i, ConfigGetFlag(i));
      }
      sleep(1);
    }
    if ((!strcmp(pArguments[0], "param")) || (!strcmp(pArguments[0], "all")))
    {
      for (int i = 0; i < CONFIG_MAX_PARAMS; i++)
      {
        ConsolePrintf("param %d: %d\n\r", i, ConfigGetParam(i));
      }
      sleep(1);
    }
    sleep(1);
    return(CONSOLE_ERROR_OK);
  }

  ConsolePrintf("Usage: get <Config item>\n\r");
  ConsolePrintf("       Config item can be: smsnum, voicenum, gprsurl, fwurl, all\n\r");
  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Set Config data from an SMS format string
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int ConfigConsoleSMS (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    ConfigHandleViaSMS(pArguments[0]);
    return (CONSOLE_ERROR_OK);
  }
  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Set Config data for a hardware type and display HW version
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int SetHardwareControlData (int argCount, char **pArguments)
{
  int data = 0;
  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    /* get the value*/
    sscanf(pArguments[0],  "%X", &data);
    /* check for valid address */
    if ((data < 0) || (data > 0xFF))
    {
      return (CONSOLE_ARG_RANGE);
    }
    /* write to RTC register */
    RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, data);
    ConsolePrintf("Hardware type set to = 0x%02X\n\r", RtcBkupReadRegister(RTC_BACKUP_HARDWARE_TYPE));
    return(CONSOLE_ERROR_OK);
  }
  ConsolePrintf("Current Hardware type = 0x%02X,  HW version = %d\n\r", RtcBkupReadRegister(RTC_BACKUP_HARDWARE_TYPE), RtcBkupReadRegister(RTC_BACKUP_HARDWARE_VERSION));
  return(CONSOLE_ARG_NO);
}

