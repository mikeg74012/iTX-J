/**
 **************************************************************************************************
 * @file   OtaSerial.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  OTA transfer of new image file via serial port / console
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


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */



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
bool OtaSerialGetManifest (void)
{
  bool success = false;

  /**/


  return success;
}


/**
 * @brief     Get the update image file via HTTP(S)
 * @param
 * @return    true on success, false otherwise
 */
bool OtaSerialGetImageFile (void)
{
  bool success = false;

  return success;
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
int OtaSerialConsoleGetManifest  (int argCount, char **pArguments)
{
  /* get the manifest file from the URL in the config store */
  OtaSerialGetManifest();

  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Get the upgrade image file via HTTP(S)
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int OtaSerialConsoleGetImageFile (int argCount, char **pArguments)
{
  /* get the manifest file from the URL in the config store */
  OtaSerialGetImageFile();

  return (CONSOLE_ERROR_OK);
}

