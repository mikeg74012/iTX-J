/**
 **************************************************************************************************
 * @file   OtaHttp.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  OTA transfer of new image file via HTTP(S)
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
#define MAX_OTA_URL_LENGTH     128


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static char OtaUrl[MAX_OTA_URL_LENGTH];


/*
 *
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */



/*
 **************************************************************************************************
 *                                      PUBLIC SECTION / API
 **************************************************************************************************
 */

/**
 * @brief     Get the upgrade manifest file via HTTP(S) using the URL provided
 * @param
 * @return    true on success, false otherwise
 */
void OtaHttpGetManifest (void)
{
  /* build the full URL */


  #ifdef GD32F30X_HD
    if (ConfigGetFlag(CONFIG_FLAG_FW_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/manifest_gd32.txt", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, 1024, SERIAL_OTA_MANIFEST_AREA_START);
    }
    else if (ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/manifestbl_gd32.txt", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, 1024, SERIAL_OTA_BL_MANIFEST_AREA_START);
    }
  #else
    if (ConfigGetFlag(CONFIG_FLAG_FW_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/manifest.txt", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, 1024, SERIAL_OTA_MANIFEST_AREA_START);
    }
    else if (ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/manifestbl.txt", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, 1024, SERIAL_OTA_BL_MANIFEST_AREA_START);
    }
  #endif

}


/**
 * @brief     Get the update image file via HTTP(S)
 * @param
 * @return    true on success, false otherwise
 */
void OtaHttpGetImageFile (void)
{
  /* build the full URL */

  #ifdef GD32F30X_HD
    if (ConfigGetFlag(CONFIG_FLAG_FW_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/application_gd32.bin", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, (248 * 1024), SERIAL_OTA_DATA_AREA_START);
    }
    else if (ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/bootloader_gd32.bin", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, (63 * 1024), SERIAL_OTA_BL_DATA_AREA_START);
    }
  #else
    if (ConfigGetFlag(CONFIG_FLAG_FW_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/application.bin", ConfigGetFwUpdateUrl());
     /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, (248 * 1024), SERIAL_OTA_DATA_AREA_START);
    }
    else if (ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE))
    {
      snprintf(OtaUrl, (size_t) MAX_OTA_URL_LENGTH, "%s/bootloader.bin", ConfigGetFwUpdateUrl());
      /* download the file and put it in SPI SRAM */
      ModemHttpDownload(OtaUrl, (63 * 1024), SERIAL_OTA_BL_DATA_AREA_START);
    }
  #endif
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Get the manifest file via HTTP(S)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int OtaHttpConsoleGetManifest  (int argCount, char **pArguments)
{
  /* get the manifest file from the URL in the config store */
  OtaHttpGetManifest();

  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Get the upgrade image file via HTTP(S)
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OtaHttpConsoleGetImageFile (int argCount, char **pArguments)
{
  /* get the manifest file from the URL in the config store */
  OtaHttpGetImageFile();

  return (CONSOLE_ERROR_OK);
}
