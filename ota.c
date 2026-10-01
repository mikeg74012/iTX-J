/**
 **************************************************************************************************
 * @file   ota.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Source File - Handles OTA data storage and retrieve from SRAM.
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the OTA debugging */

#define TRACE_OTA(...)

/*
#if TRACE_OTA_ENABLE
  #define TRACE_OTA(...) TRACE_COLOUR_(FG_YELLOW, BG_DEFAULT, "OTA", __VA_ARGS__)
#else
  #define TRACE_OTA(...)
#endif
*/

#define MAXOTABLIMAGESIZE                     (63 * 1024)   /* 63 kB MAX OTA Image */
#define MAX_BL_PAGES_TO_PROGRAM               (MAXOTABLIMAGESIZE / (FLASH_PAGE_SIZE))
/* calculate the number of flash pages to program.  NOTE: We always round UP to the next whole page */
#define GET_NO_PAGES_TO_PROGRAM(otaImgSize)   ((otaImgSize + (FLASH_PAGE_SIZE - 1)) / (FLASH_PAGE_SIZE))
#define FIRMWAREUPDATEFLAG                    234
#define BOOTLOADERUPDATEFLAG                  123

/* size of buffer to use for verification */
#define VERIFY_BUFFER_LENGTH                  80
#define MANIFEST_LINE_LENGTH                  64

#define OTA_APP_UPDATE                        0
#define OTA_BL_UPDATE                         1

#define BL_VERSION_SIGNATURE                  0xA5A5A5A5  // Chosen unique magic number

typedef struct 
{
  uint32_t signature;           /* Magic number to validate the structure */
  uint32_t bootloaderVersion;   /* Bootloader version */
  uint32_t checksum;            /* CRC Checksum */
} blVersionData_t;

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static uint8_t  VerifyBuffer[VERIFY_BUFFER_LENGTH];
static char     ManifestLine[MANIFEST_LINE_LENGTH];
static uint32_t ManifestAddress = SERIAL_OTA_MANIFEST_AREA_START;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool    ReadOTAInfoFromSpiRAM      (OTA_Info_t *pBuffer, uint8_t otaMode);
static bool    WriteOTAInfoToSpiRAM       (OTA_Info_t *pBuffer, uint8_t otaMode);
static bool    OTAImageVerifyUpgradeImage (uint32_t imageSize, uint32_t expectedCRC);
static bool    OtaManifestReadLine        (void);
static bool    UpdateBootloader           (void);
static void    OtaUpgradeNow              (void);
static void    OtaUpgradeBLNow            (void);
static bool    ReadOTAData16FromSpiRAM    (const uint32_t datastorageindex, uint16_t *pBuffer);
static bool    VerifyFlashedBL            (uint32_t blsize);
static uint32_t GenerateBLVersionCrc      (blVersionData_t *pData);
static uint16_t GetCombinedFWVersion      (void);
/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Process an OTA manifest file
 * @param     None
 * @return    true if verify successful, false otherwise
 */
bool OtaProcessManifest (void)
{
  OTA_Info_t otaInfo = { 0 };
  uint8_t otaMode    = OTA_APP_UPDATE;
  /* reset Manifest address to star of file */
  ManifestAddress = SERIAL_OTA_MANIFEST_AREA_START;
  if ((ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE)) && (!ConfigGetFlag(CONFIG_FLAG_FW_UPDATE)))
  {
    ManifestAddress = SERIAL_OTA_BL_MANIFEST_AREA_START;
    otaMode = OTA_BL_UPDATE;
  }

  /* scan the manifest one line at a time and process contents */
  for (int i = 0; i < 16; i++)
  {
    /* read a line from the manifest file */
    OtaManifestReadLine();
    TRACE_OTA("MAN->%s\n\r", ManifestLine);

    /* files starting with '#' are ignored as comments */
    if ('#' != ManifestLine[0])
    {
      if (strstr(ManifestLine, "CRC") != NULL)
      {
        if (sscanf(ManifestLine,"CRC %x", (unsigned int *) &otaInfo.FileCrc) != 1)
        {
          TRACE_OTA("Could not read CRC from Manifest\n\r");
          otaInfo.FileCrc = 0;
        }
      }
      if (strstr(ManifestLine, "SIZE") != NULL)
      {
        if (sscanf(ManifestLine,"SIZE %d", (unsigned int *) &otaInfo.FileSize) != 1)
        {
          TRACE_OTA("Could not read file size from Manifest\n\r");
          otaInfo.FileSize = 0;
        }
      }
      if (strstr(ManifestLine, "EOF") != NULL)
      {
        break;
      }
    }
  }
  /* write the decoded manifest details to the OTA info area of SPI SRAM */
  otaInfo.UpdateFlag = 0;
  WriteOTAInfoToSpiRAM(&otaInfo, otaMode);

  return true;
}


/**
 * @brief     Verify the OTA image
 * @param     None
 * @return    true if verify successful, false otherwise
 */
bool OtaVerify (void)
{
  bool result = false;
  uint8_t otaMode = OTA_APP_UPDATE;
  /* Read the OTA manifest info from SPI SRAM */
  OTA_Info_t otaInfo = { 0 };

  if ((ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE)) && (!ConfigGetFlag(CONFIG_FLAG_FW_UPDATE)))
  {
    otaMode = OTA_BL_UPDATE;
  }

  ReadOTAInfoFromSpiRAM(&otaInfo, otaMode);

  /* Check OTA Image CRC Checksum */
  if (OTAImageVerifyUpgradeImage(otaInfo.FileSize, otaInfo.FileCrc))
  {
    TRACE_OTA("Verify OTA Image: PASSED\n\r");
    result = true;
    if (otaMode == OTA_BL_UPDATE)
    {
      /* set update flag to let APP now bootloader has verified */
      otaInfo.UpdateFlag = BOOTLOADERUPDATEFLAG;
      WriteOTAInfoToSpiRAM(&otaInfo, OTA_BL_UPDATE);
    }
  }
  else
  {
    TRACE_OTA("Verify OTA Image: FAILED\n\r");
  }
  return result;
}


/**
 * @brief     Program the OTA image
 * @param     None
 * @return    true if verify successful, false otherwise
 */
void OtaProgram (void)
{
  /* clear the OTA request flag */
  if ((ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE)) && (!ConfigGetFlag(CONFIG_FLAG_FW_UPDATE)))
  {
   sleep(1000);
   OtaUpgradeBLNow();  /* not return when reboot */
  }
  else
  {
   ConfigSetFlag(CONFIG_FLAG_FW_UPDATE, false);
   /* record current FW version before update */
     RtcBkupWriteRegister(RTC_BACKUP_FIRMWARE_VERSION, GetCombinedFWVersion());
   #if DEBUG
     ConsolePrintf("---> OTA P[%d] \n\r", RtcBkupReadRegister(RTC_BACKUP_FIRMWARE_VERSION));
   #endif
   /* reboot the mcu */
   sleep(1000);
   OtaUpgradeNow();  /* not return to here */
  }
}


/**
 * @brief     Check the OTA FW updated stay at the same version
 * @param     None
 * @return    true if OTA updated to a different version
 */
bool OTAUpdateFWVerify  (void)
{
  uint16_t fwVersion = 0;
  uint16_t prefwVersion = 0;

  fwVersion = GetCombinedFWVersion();
  prefwVersion = RtcBkupReadRegister(RTC_BACKUP_FIRMWARE_VERSION);
  #if DEBUG
    ConsolePrintf("---> OTA FW P[%d] [%d] \n\r", prefwVersion, fwVersion);
  #endif
  if (prefwVersion == fwVersion)
  {
    /* log the event if OTA update to same version number */
    StatusLogWriteEventCode(STATUS_OTA_FW_SAMEVERSION, fwVersion);
    return (false);
  }
  else
  {
    return (true);  /* updated to a different version */
  }
}


/**
 * @brief     Get the Bootloader Version to the SPI RAM
 * @param     None
 * @return    uint32_t bootloader version in the lowest 3 bytes;
 */
uint32_t GetBLVersionFromSpiRAM (void)
{
  bool success = false;
  blVersionData_t blData;
  success = SerialRamRead(SERIAL_RAM_COMMON_AREA_START, (uint8_t*)&blData, sizeof(blVersionData_t));

  if (!success)
  {
    TRACE_OTA("Failed to read Bootloader Version from SPI RAM\n\r");
  }
  
  if (blData.signature != BL_VERSION_SIGNATURE)
  {
    TRACE_OTA("Invalid signature in Bootloader Version data.\n\r");
    return 0;
  }

  /* Validate CRC */
  uint32_t calculatedCrc = GenerateBLVersionCrc(&blData);
  if (calculatedCrc != blData.checksum)
  {
      TRACE_OTA("Bootloader Version CRC mismatch! Data may be corrupted.\n\r");
      return 0;
  }

  return (blData.bootloaderVersion & 0x00ffffff);
}

/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */
/**
 * @brief     Reboot the device
 * @param     None
 * @return    None
 * @warning   Never returns
 */
static void OtaUpgradeNow (void)
{
  /* Read the OTA manifest info from SPI SRAM */
  OTA_Info_t otaInfo = { 0 };
  ReadOTAInfoFromSpiRAM(&otaInfo, OTA_APP_UPDATE);

  /* set update flag to tell the bootloader a new firmware is available */
  otaInfo.UpdateFlag = FIRMWAREUPDATEFLAG;
  WriteOTAInfoToSpiRAM(&otaInfo, OTA_APP_UPDATE);

  /* Set the OTA FW update reset cause Flag to the reset backup register*/
  RtcBkupWriteRegister(RTC_BACKUP_REGISTER_RESET_CAUSE, RESET_FW_UPDATE);

  /*
  * Reboot the MCU to start the firmware upgrade process
  * WARNING - Never returns
  */
  SystemReset();
}


/**
 * @brief     update boot loader
 *
 * @param     None
 * @return    None
 * @warning   Never returns
 */
static void OtaUpgradeBLNow (void)
{
  /* Read the OTA manifest info from SPI SRAM */
  OTA_Info_t otaInfo = { 0 };
  ReadOTAInfoFromSpiRAM(&otaInfo, OTA_BL_UPDATE);
  TRACE_OTA("OTA update flags: %d\n\r", otaInfo.UpdateFlag);
  if (otaInfo.UpdateFlag == BOOTLOADERUPDATEFLAG)
  {
    /* Write PSRAM data to FLASH, Programming */
    if (UpdateBootloader())
    {
      /* Could verify the BL flash here before restart */
      if (VerifyFlashedBL(otaInfo.FileSize))
      {
        /* Set the OTA FW update reset cause Flag to the reset backup register*/
        RtcBkupWriteRegister(RTC_BACKUP_REGISTER_RESET_CAUSE, RESET_BL_UPDATE);
        /* MCU reboot */
        ConsolePrintf("!!! MCU Resetting now !!!\n\r");
        sleep(100);
        SystemReset();    /* No return to here */
      }
      else
      {
        /* log the BL flash verify error, re-issue flag to try again from next DHM */
        StatusLogWriteEventCode(STATUS_OTA_BL_VERIFY_ERR, STATUS_DATA_LOAD_NONE);
      }
    }
    else
    {
      TRACE_OTA("OTA bootloader update failed ! \n\r");
      /* program BL failed,  re-issue flag and try again from next DHM */
      StatusLogWriteEventCode(STATUS_OTA_BL_PROGRAM_ERR, STATUS_DATA_LOAD_NONE);
    }
  }
}


static bool OtaManifestReadLine (void)
{
  char NextChar;
  int Index = 0;

  /* read from the manifest file until /r or max line length reached */
  for (Index = 0; Index < MANIFEST_LINE_LENGTH; )
  {
    SerialRamRead(ManifestAddress++, (uint8_t *) &NextChar, 1);
    if ((0x0d != NextChar) && (0x0a != NextChar))
    {
      ManifestLine[Index++] = NextChar;
    }
    if (0x0d == NextChar) break;
  }

  /* force null terminator in last position for safety */
  ManifestLine[Index] = 0;
  return (Index > 0);
}


/**
 * @brief     Write OTA manifest info to SPI RAM
 * @param     pBuffer   : Pointer to a OTA_Info_t data to write
 * @param     otaMode   : 0:App, 1:Bootloader
 * @return    True if successful, false otherwise
 */
static bool WriteOTAInfoToSpiRAM (OTA_Info_t *pBuffer, uint8_t otaMode)
{
  bool success = false;
  uint32_t ramAddress = (otaMode == OTA_BL_UPDATE)? SERIAL_OTA_BL_INFO_AREA_START: SERIAL_OTA_INFO_AREA_START;

  success = SerialRamWrite(ramAddress, (uint8_t*) pBuffer, sizeof(OTA_Info_t));

  if (!success)
  {
    TRACE_OTA("Failed to write OTA manifest info at SPI RAM\n\r");
  }

  return success;
}


/**
 * @brief     Read OTA manifest information from SPI RAM
 * @param     pBuffer   : Pointer to a OTA_Info_t data to write
 * @param     otaMode   : 0:App, 1:Bootloader
 * @return    true on success, false otherwise
 */
static bool ReadOTAInfoFromSpiRAM (OTA_Info_t *pBuffer, uint8_t otaMode)
{
  bool success = false;
  uint32_t readAddress = (otaMode == OTA_BL_UPDATE)? SERIAL_OTA_BL_INFO_AREA_START: SERIAL_OTA_INFO_AREA_START;

  if (pBuffer != NULL)
  {
    success = SerialRamRead(readAddress, (uint8_t*) pBuffer, sizeof(OTA_Info_t));
  }
  if (!success)
  {
    TRACE_OTA("Failed to read OTA manifest info from SPI SRAM\n\r");
  }

  return success;
}


/**
 * @brief    Read and Verify OTA Full Image data from SPI SRAM
 * @param    imageSize   : Size of the firmware image in bytes
 * @param    expectedCRC : The expected CRC across the whol image
 * @return   true on success, false on failure
 */
static bool OTAImageVerifyUpgradeImage (uint32_t imageSize, uint32_t expectedCRC)
{
  uint32_t sramAddress = SERIAL_OTA_DATA_AREA_START;
  int TransferLength;
  uint32_t CalculatedCRC = 0;
  /* check if it is a BL update */
  if ((ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE)) && (!ConfigGetFlag(CONFIG_FLAG_FW_UPDATE)))
  {
    sramAddress = SERIAL_OTA_BL_DATA_AREA_START;
  }
  /* initialise the CRC driver */
  CrcDriverInitCrc32Multi();

  /*
   * Read upgrade image one block at a time
   */
  while (imageSize)
  {
    /* calculate the transfer length */
    if (imageSize > VERIFY_BUFFER_LENGTH)
    {
      TransferLength = VERIFY_BUFFER_LENGTH;
    }
    else
    {
      TransferLength = imageSize;
    }

    /* get the next block of data from SPI SRAM */
    memset((void *)VerifyBuffer, 0, VERIFY_BUFFER_LENGTH);
    SerialRamRead(sramAddress, &VerifyBuffer[0], TransferLength);

    /* Add this block to the CRC calculation */
    CalculatedCRC = CrcDriverCalculateCrc32Multi(VerifyBuffer, TransferLength);

    /* update the transfer details */
    sramAddress += TransferLength;
    imageSize -= TransferLength;
  }

  CalculatedCRC = CrcDriverFinishCrc32Multi(CalculatedCRC);

  /* Check that calculated CRC matches the expected CRC */
  TRACE_OTA("CRC expected = %08x, calculated = %08x\n\r", expectedCRC, CalculatedCRC);
  return (CalculatedCRC == expectedCRC);
}

/**
 * @brief     Generate a checksum from the bootloader version data
 * @param     pData - pointer to bootloader version data
 * @return    CRC - the calculated checksum
 */
static uint32_t GenerateBLVersionCrc(blVersionData_t *pData)
{
    /* Compute CRC over the signature and bootloaderVersion, excluding the checksum field */
    uint32_t dataLen = sizeof(pData->signature) + sizeof(pData->bootloaderVersion);
    return CrcDriverCalculateCrc32((uint8_t *)pData, dataLen);
}


/**
 * @brief  This function performs an update of the device bootloader from the SRAM image
 * @param  None
 * @retval true for successful BL program
 */
static bool UpdateBootloader (void)
{
  bool rtnValue = false;
  /* Now read OTA info from SRAM area */
  OTA_Info_t otaInfo = { 0 };

  /* Start by reading the manifest information */
  ReadOTAInfoFromSpiRAM(&otaInfo, OTA_BL_UPDATE);

  if ((0 < otaInfo.FileSize) && (otaInfo.FileSize < MAXOTABLIMAGESIZE))
  {
    /* calculate the number of FLASH pages we need to erase and reprogram */
    uint32_t NoPagesToProgram = GET_NO_PAGES_TO_PROGRAM(otaInfo.FileSize);

    /* Check OTA Image CRC Checksum again before triggering update */
    if (OTAImageVerifyUpgradeImage(otaInfo.FileSize, otaInfo.FileCrc))
    {
      TRACE_OTA("\n\r");
      TRACE_OTA("Bootloader CRC verify: PASSED\n\r");
      TRACE_OTA("Starting the upgrade process\n\r");
      TRACE_OTA("- Image size:  %d bytes\n\r", otaInfo.FileSize);
      TRACE_OTA("- CRC:         %08x\n\r", otaInfo.FileCrc);
      TRACE_OTA("- FLASH pages: %d\n\r", NoPagesToProgram);

      /* Check Flash program area */
      if ((NoPagesToProgram > 0) && (NoPagesToProgram <= MAX_BL_PAGES_TO_PROGRAM))
      {
        /* Erase the bootloader area */
        rtnValue = FlashErase (BOOTLOADER_ADDRESS, NoPagesToProgram);
        TRACE_OTA("Erasing bootloader area... %s\n\r", rtnValue? "OK":"FAILED");

        /* Evaluate flash program parameters */
        static uint16_t otaPointer = 0;
        int sizeInHalfWords = ((otaInfo.FileSize + 1) / 2);
        uint32_t currentFlashAddr = BOOTLOADER_ADDRESS;

        /* programming */
        for (int i = 0; i < sizeInHalfWords; i++)
        {
          /* Now copy from OTA SRAM area */
          if (ReadOTAData16FromSpiRAM(i, &otaPointer))
          {
            if (!FlashWrite(currentFlashAddr, (uint8_t *) &otaPointer, 2))
            {
              TRACE_OTA("16 bit Flash Write failed\n\r");
              rtnValue = false;
              break;
            }
          }
          else  /* read failed */
          {
            TRACE_OTA("Read PSRAM failed\n\r");
            rtnValue = false;
            break;
          }
          currentFlashAddr += 2;
        }
      }
      else
      {
        TRACE_OTA("OTA Image size error!\n\r");
      }
    }
    else
    {
      TRACE_OTA("OTA image CRC:    %08x\n\r", otaInfo.FileCrc);
      TRACE_OTA("Verify OTA Image: FAILED\n\r");
    }
  }
  else
  {
    TRACE_OTA("OTA image size:   %d\n\r", otaInfo.FileSize);
    TRACE_OTA("OTA Image size over range!\n\r");
  }

  if (rtnValue)
  {
    /* set flag to 0 to avoid repeating */
    otaInfo.UpdateFlag = 0;
    WriteOTAInfoToSpiRAM(&otaInfo, OTA_BL_UPDATE);
    /* reset the OTA BL config flag when it done */
    ConfigSetFlag(CONFIG_FLAG_OTA_BL_UPDATE, false);
  }

  TRACE_OTA("Programming ...            %s\n\r", rtnValue? "OK": "FAILED");
  return (rtnValue);
}


/**
 * @brief     Read an OTA 16 bit data from SPI RAM BL data block
 * @param     The storage index to use
 * @param     Pointer to a buffer to hold the data
 * @return    True if successful, false otherwise
 */
static bool ReadOTAData16FromSpiRAM (const uint32_t datastorageindex, uint16_t *pBuffer)
{
  bool success = false;

  if (pBuffer != NULL)
  {
    if (SerialRamRead(SERIAL_OTA_BL_DATA_AREA_START + (2 * datastorageindex), (uint8_t*) pBuffer, sizeof(uint16_t)))
    {
      success = true;
    }
  }
  if (!success)
  {
    TRACE_OTA("Failed to read from SPI SRAM\n\r");
  }

  return success;
}


/**
 * @brief     verify BL flash with PSRAM
 * @param     blsize  :BL byte count
 * @return    True if they are the same, false otherwise
 */
static bool  VerifyFlashedBL (uint32_t blsize)
{
  bool success = true;
  int TransferLength;
  uint8_t  BLflashBuffer[VERIFY_BUFFER_LENGTH];
  uint32_t sramAddress = SERIAL_OTA_BL_DATA_AREA_START;
  uint32_t blAddress   = BOOTLOADER_ADDRESS;
  uint32_t imageSize   = blsize;

  /*
   * Read upgrade image one block at a time
   */
  while (imageSize)
  {
    /* calculate the transfer length */
    if (imageSize > VERIFY_BUFFER_LENGTH)
    {
      TransferLength = VERIFY_BUFFER_LENGTH;
    }
    else
    {
      TransferLength = imageSize;
    }

    /* get the next block of data from SPI SRAM */
    SerialRamRead(sramAddress, &VerifyBuffer[0], TransferLength);
    /* read the BL flash */
    memcpy(BLflashBuffer, (uint16_t *)blAddress, TransferLength);
    for(uint8_t i = 0; i<TransferLength; i++ )
    {
      if (BLflashBuffer[i] != VerifyBuffer [i])
      {
        ConsolePrintf("\r\nFailed@%d:[%d][%d]\r\n",i, BLflashBuffer[i],VerifyBuffer [i]);
        success = false;
        break;
      }
    }
    if (!success)
    {
      break;
    }
    sramAddress += TransferLength;
    blAddress   += TransferLength;
    imageSize   -= TransferLength;
  }
  TRACE_OTA("Verify BL Flash ...        %s\n\r", success? "PASSED": "FAILED");
  return (success);
}


/**
 * @brief     return combined FW version number = maj*10000+min*100 + pat
 * @param     None
 * @return    uint16 version number > 0
 */
static uint16_t GetCombinedFWVersion (void)
{
  uint16_t fwVersion = 0;
  int maj,min,pat;

  if (sscanf((char*)FIRMWARE_VERSION_STRING, "%d.%d.%d", &maj, &min, &pat) == 3)
  {
    fwVersion = (maj * 1000) + (min*100) + pat;
  }

  return (fwVersion);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Trigger an OTA firmware Upgrade using image from SRAM
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleUpgradeNow (int ArgCount, char **pArguments)
{
  /*
   * Start the firmware upgrade process
   * WARNING - Never returns
   */
  OtaUpgradeNow();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Console command to read and verify OTA Full Image data on SRAM
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleVerify (int argCount, char **pArguments)
{
  OtaVerify();

  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Write the given OTA Image data information in SRAM
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleSetInfo (int argCount, char **pArguments)
{
  int FileSize;
  int FileCrc;

  /* check for correct number of arguments */
  if (2 == argCount)
  {
    /* get the file size and crc */
    sscanf(pArguments[0], "%d", &FileSize);
    sscanf(pArguments[1], "%x", &FileCrc);
    uint8_t otaMode = OTA_APP_UPDATE;

    if ((ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE)) && (!ConfigGetFlag(CONFIG_FLAG_FW_UPDATE)))
    {
      otaMode = OTA_BL_UPDATE;
    }
    /* Read the current OTA info from SRAM area */
    OTA_Info_t otaDataInfo = { 0 };
    ReadOTAInfoFromSpiRAM(&otaDataInfo, otaMode);

    /* Update the file size and CRC */
    otaDataInfo.FileSize = (uint32_t) FileSize;
    otaDataInfo.FileCrc  = (uint32_t) FileCrc;

    /* Write the updated OTA info back to SRAM */
    if (!WriteOTAInfoToSpiRAM(&otaDataInfo, otaMode))
    {
      TRACE_OTA("Error writing OTA data info\n\r");
    }
    return (CONSOLE_ERROR_OK);
  }

  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Display the OTA image information
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleGetInfo (int argCount, char **pArguments)
{
  /* Read the current OTA info from SRAM area */
  OTA_Info_t otaDataInfo = { 0 };
  ReadOTAInfoFromSpiRAM(&otaDataInfo, OTA_APP_UPDATE);

  /* Display the file size and CRC */
  ConsolePrintf(" OTA image size:   %d\n\r", otaDataInfo.FileSize);
  ConsolePrintf(" OTA image CRC:    %08x\n\r", otaDataInfo.FileCrc);
  ConsolePrintf(" OTA update flags: %d\n\r", otaDataInfo.UpdateFlag);

  ConsolePrintf(" Bootloader version: 0x%x\n\r", GetBLVersionFromSpiRAM());
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Process the manifest file
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleProcessManifest (int argCount, char **pArguments)
{
  OtaProcessManifest();
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Trigger an OTA bootloader Upgrade using image from SRAM
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int OTAImageConsoleUpdateBL (int ArgCount, char **pArguments)
{
  /*
   * Start the firmware upgrade process
   */
  OtaUpgradeBLNow();
  /* indicate success */
  return (CONSOLE_ERROR_OK);
}
