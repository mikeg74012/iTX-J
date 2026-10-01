/**
 **************************************************************************************************
 * @file   configupdate.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   24/03/2020
 *
 * @brief  Handles the update of Config Data
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
#define ASCII_0                    48

#define VERSION_CHAR_1             '0'
#define VERSION_CHAR_2             '1'


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static configData_t configData;
static bool HibernateRequested = false;
static bool ResetRequested = false;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool handleTLV (configData_t *configData,
                       int blockType,
                       int blockLength,
                       char *blockValue);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Read a config value from block ID
 * @param     blockId : block ID of the config item to read
 * @param     blockParam: used to identify individual flags and parameters.  Not used for other blocks.
 * @param     pBuf : buffer to copy the data into (as null terminated string)
 * @param     bufLen : max length of the buffer
 * @return    true on success, false on failure
 */
bool ConfigTextGetValue (uint8_t blockId, uint8_t blockParam, char *pBuf, uint8_t bufLen)
{
  bool     result     = true;
  char     TempBuf[16];
  char *   pText      = NULL;
  uint8_t  CustomerId = 0;
  uint32_t IpPort     = 0;
  uint8_t  FlagValue  = 0;
  uint32_t ParamValue = 0;

  /* Get or create the requested text */
  switch (blockId)
  {
    case OUTGOING_SMS_TYPE:
      pText = ConfigGetSmsNumber();
      break;
    case VOICECALL_NO_TYPE:
      pText = ConfigGetVoiceNumber();
      break;
    case GPRS_URL_TYPE:
      pText = ConfigGetGprsUrl();
      break;
    case IP_ADDRESS_TYPE:
      pText = ConfigGetIpAddress();
      break;
    case IP_PORT_TYPE:
      IpPort = ConfigGetIpPort();
      sprintf(TempBuf, "%d", (int) IpPort);
      pText = TempBuf;
      break;
    case FW_UPDATE_URL_TYPE:
      pText = ConfigGetFwUpdateUrl();
      break;
    case CUSTOMER_TYPE:
      CustomerId = ConfigGetCustomer();
      sprintf(TempBuf, "%d", (int) CustomerId);
      pText = TempBuf;
      break;
    case APN_URL_TYPE:
      pText = ConfigGetApnUrl();
      break;
    case APN_USERNAME_TYPE:
      pText = ConfigGetApnUsername();
      break;
    case APN_PASSWORD_TYPE:
      pText = ConfigGetApnPassword();
      break;
    case FLAG_TYPE:
      FlagValue = ConfigGetFlag(blockParam);
      sprintf(TempBuf, "%02d%d", (int) blockParam, (int) FlagValue);
      pText = TempBuf;
      break;
    case PARAMETER_TYPE:
      ParamValue = ConfigGetParam(blockParam);
      sprintf(TempBuf, "%02d%d", (int) blockParam, (int) ParamValue);
      pText = TempBuf;
      break;
    case HIBERNATE_REQ_TYPE:
    case RESET_REQ_TYPE:
    default:
      pText = "";
      result = false;
      break;
  }

  /* Copy the text into the callers buffer */
  strncpy(pBuf, pText, bufLen);
  pBuf[bufLen - 1] = 0;
  return (result);
}



/**
 * @brief     Update the Config Data from an SMS string
 * @param     The SMS string
 * @return    Whether the update was successful
 */
bool ConfigUpdateConfigFromSms (char *smsString)
{
  bool returnVal = false;
  bool parsedOK = true;
  char blockValue[MAX_SMS_LENGTH];

  if (ConfigReadData(&configData) == FLASH_OPERATION_SUCCESS)
  {
    /* Parse SMS */
    uint16_t stringLength = strnlen(smsString, MAX_SMS_LENGTH);
    uint16_t stringIndex = 0;

    if (stringLength > 2)
    {
      /* Check version chars */
      if ((smsString[0] == VERSION_CHAR_1) && (smsString[1] == VERSION_CHAR_2))
      {
        stringIndex += 2;
        /* Go through each TLV in turn */
        for (int i=0; i < CONFIG_MAX_TLVS; i++)
        {
          int blockType;
          int blockLength;
          /* Read type - in the next 2 chars*/
          if (stringLength > (stringIndex+1))
          {
            if (sscanf(&smsString[stringIndex], "%2d", &blockType) == 1)
            {
              if (blockType < CONFIG_MAX_TLVS)
              {
                /* Proceed */
                stringIndex += 2;
              }
              else
              {
                ConsolePrintf("Block type out of range %d\n\r", blockType);
                parsedOK = false;
                break;
              }
            }
            else
            {
              /* Type is not an integer. */
              ConsolePrintf("Block type not an integer %c%c\n\r",
                  smsString[stringIndex], smsString[stringIndex+1]);
              parsedOK = false;
              break;
            }
          }
          else
          {
            /* String has ended unexpectedly */
            ConsolePrintf("Unexpected end of string at index %d\n\r", stringIndex);
            parsedOK = false;
            break;
          }

          if (blockType == TERMINATOR_TYPE)
          {
            /* Finish TLV loop and proceed to CRC */
            break;
          }

          /* Read length - in the next 3 chars */
          if (stringLength > (stringIndex+2))
          {
            if (sscanf(&smsString[stringIndex], "%3d", &blockLength) == 1)
            {
              /* Proceed */
              stringIndex += 3;
            }
            else
            {
              /* Length is not an integer. */
              ConsolePrintf("Block length not an integer %c%c%c\n\r",
                  smsString[stringIndex], smsString[stringIndex+1], smsString[stringIndex+2]);
              parsedOK = false;
              break;
            }
          }
          else
          {
            /* String has ended unexpectedly */
            ConsolePrintf("Unexpected end of string at index %d\n\r", stringIndex);
            parsedOK = false;
            break;
          }

          /* Read Value out of string */
          if (blockLength > 0) // zero length can be valid
          {
            if (stringLength > (stringIndex+blockLength-1))
            {
              memcpy(blockValue, &smsString[stringIndex], blockLength);
              blockValue[blockLength] = '\0';
              stringIndex += blockLength;
            }
            else
            {
              ConsolePrintf("Block Length %d too long!\n\r", blockLength);
              parsedOK = false;
              break;
            }
          }
          if (!handleTLV(&configData, blockType, blockLength, blockValue))
          {
            parsedOK = false;
            break;
          }
        }//endfor TLVs
      }
      else
      {
        parsedOK = false;
        ConsolePrintf("Incorrect version number %c%c\n\r", smsString[0], smsString[1]);
      }// if version chars
    }
    else
    {
      /* Too short! */
      ConsolePrintf("String too short\n\r");
      parsedOK = false;
    }

    if (parsedOK)
    {
      /* Check if anything has changed and write new config to FLASH if it has */
      if (ConfigCompareWithCurrent(&configData, false))
      {
        ConsolePrintf("Config not changed\n\r");
      }
      else
      {
        /* Write config data back to EEPROM */
        if (ConfigWriteData(&configData) == FLASH_OPERATION_SUCCESS)
        {
          ConsolePrintf("Config updated successfully\n\r");
        }
        else
        {
          ConsolePrintf("Error writing config\n\r");
        }
      }
      /* if hibernate has been requested, do it now... */
      if (HibernateRequested)
      {
        /*
         * Hibernate the device
         * WARNING - Never returns
         */
        ConsolePrintf("Going into hibernation!!!\n\r");
        MotionMonitorHibernate();
      }
      /* if a reset has been requested, do it now... */
      if (ResetRequested)
      {
        /*
         * Reboot the MCU
         * WARNING - Never returns
         */
        PowerResetMCU();
      }
    }
  }
  return returnVal;
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     Handle the config update for a single TLV block
 * @param     Pointer to a copy of the config data
 * @param     The type of block
 * @param     The data length in the block
 * @param     The data in the block
 * @param     Pointer to Hibernate Request flag
 * @return    Whether the data was valid
 */
static bool handleTLV (configData_t *configData,
                       int blockType,
                       int blockLength,
                       char *blockValue)
{
  bool returnVal = false;

  switch(blockType)
  {
  case CUSTOMER_TYPE:
    if (blockLength <= 3)
    {
      int CustomerNumber;
      sscanf(blockValue, "%d", (&CustomerNumber));
      configData->Customer = (uint8_t) CustomerNumber;
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Customer value too long!\n\r");
    }
    break;

  case OUTGOING_SMS_TYPE:
    if (blockLength <= MAX_MOBILE_NUMBER_SIZE)
    {
      memcpy(configData->SmsNumber, blockValue, blockLength);
      configData->SmsNumber[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Outgoing SMS too long!\n\r");
    }
    break;

  case VOICECALL_NO_TYPE:
    if (blockLength <= MAX_MOBILE_NUMBER_SIZE)
    {
      memcpy(configData->VoiceCallNumber, blockValue, blockLength);
      configData->VoiceCallNumber[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Voicecall number too long!\n\r");
    }
   break;

  case GPRS_URL_TYPE:
    if (blockLength <= MAX_URL_SIZE)
    {
      memcpy(configData->GprsUrl, blockValue, blockLength);
      configData->GprsUrl[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("GPRS URL too long!\n\r");
    }
    break;

  case IP_ADDRESS_TYPE:
    if (blockLength <= MAX_IP_ADDRESS_SIZE)
    {
      memcpy(configData->IpAddress, blockValue, blockLength);
      configData->IpAddress[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("IP address too long!\n\r");
    }
    break;

  case IP_PORT_TYPE:
    if (blockLength <= 6)
    {
      sscanf(blockValue, "%d", (int *)(&configData->IpPort));
      returnVal = true;
    }
    else
    {
      ConsolePrintf("IP port too long!\n\r");
    }
    break;

  case FW_UPDATE_URL_TYPE:
    if (blockLength <= MAX_URL_SIZE)
    {
      memcpy(configData->FwUpdateUrl, blockValue, blockLength);
      configData->FwUpdateUrl[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("FW Update URL too long!\n\r");
    }
    break;

  case FLAG_TYPE:
    if (blockLength == 3)
    {
      uint8_t FlagId = ((blockValue[0] - ASCII_0) * 10) + (blockValue[1] - ASCII_0);
      bool FlagValue =  blockValue[2] - ASCII_0;
      if (FlagId < CONFIG_MAX_FLAGS)
      {
        configData->Flags[FlagId] = FlagValue;
      }
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Wrong block length!\n\r");
    }
    break;

  case PARAMETER_TYPE:
    if (blockLength <= 11)
    {
      uint8_t ParameterId = ((blockValue[0] - ASCII_0) * 10) + (blockValue[1] - ASCII_0);

      if (ParameterId < CONFIG_MAX_PARAMS)
      {
        sscanf(blockValue + 2, "%d", (int *)(&configData->Parameters[ParameterId]));
      }
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Wrong block length!\n\r");
    }
    break;

  case HIBERNATE_REQ_TYPE:
    if (blockLength == 0) // only valid without any accompanying data
    {
      HibernateRequested = true;
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Wrong block length!\n\r");
    }
    break;

  case RESET_REQ_TYPE:
    if (blockLength == 0) // only valid without any accompanying data
    {
      ResetRequested = true;
      returnVal = true;
    }
    else
    {
      ConsolePrintf("Wrong block length!\n\r");
    }
    break;

  case APN_URL_TYPE:
    if (blockLength <= MAX_URL_SIZE)
    {
      memcpy(configData->ApnUrl, blockValue, blockLength);
      configData->ApnUrl[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("APN URL too long!\n\r");
    }
    break;

  case APN_USERNAME_TYPE:
    if (blockLength <= MAX_USERNAME_SIZE)
    {
      memcpy(configData->ApnUsername, blockValue, blockLength);
      configData->ApnUsername[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("APN Username too long!\n\r");
    }
    break;

  case APN_PASSWORD_TYPE:
    if (blockLength <= MAX_PASSWORD_SIZE)
    {
      memcpy(configData->ApnPassword, blockValue, blockLength);
      configData->ApnPassword[blockLength]  = 0; // terminate string
      returnVal = true;
    }
    else
    {
      ConsolePrintf("APN Password too long!\n\r");
    }
    break;

  default:
    ConsolePrintf("Unexpected Block Type %d\n\r", blockType);
    break;
  }
  return returnVal;
}
