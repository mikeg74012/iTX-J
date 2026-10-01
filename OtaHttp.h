/**
 **************************************************************************************************
 * @file   OtaHttp.h
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
#ifndef _OTAHTTP_H
#define _OTAHTTP_H

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
void OtaHttpGetManifest         (void);
void OtaHttpGetImageFile        (void);

/* Console Commands */
int  OtaHttpConsoleGetManifest  (int argCount, char **pArguments);
int  OtaHttpConsoleGetImageFile (int argCount, char **pArguments);


#endif /* _OTAHTTP_H */
