/**
 **************************************************************************************************
 * @file   OtaSerial.h
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
#ifndef _OTASERIAL_H
#define _OTASERIAL_H

#include <stdint.h>
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool OtaSerialGetManifest         (void);
bool OtaSerialGetImageFile        (void);

/* Console Commands */
int  OtaSerialConsoleGetManifest  (int argCount, char **pArguments);
int  OtaSerialConsoleGetImageFile (int argCount, char **pArguments);


#endif /* _OTASERIAL_H */
