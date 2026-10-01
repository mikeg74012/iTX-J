/**
 **************************************************************************************************
 * @file   gsmHttp.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements HTTP functionality for GSM Modems.
 *
 * @notes: Only one HTTP context is currently provided.  This module could be modified to provide
 *         multiple contexts in the future
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#ifndef _GSMHTTP_H_
#define _GSMHTTP_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmHttpConfigAndSend (uint8_t * pData, uint32_t size);
bool GsmHttpConfigure     (void);
bool GsmHttpSend          (uint8_t * pData, uint32_t size);
bool GsmHttpDownloadFile  (char * url, uint32_t maxSize, uint32_t sramAddress);


/* Console Commands */
int GsmHttpConsoleConfigure (int argCount, char **pArguments);
int GsmHttpConsoleSetUrl    (int argCount, char **pArguments);
int GsmHttpConsolePost      (int argCount, char **pArguments);
int GsmHttpConsoleRead      (int argCount, char **pArguments);
int GsmHttpConsoleTest      (int argCount, char **pArguments);
int GsmHttpConsoleFlag      (int argCount, char **pArguments);
int GsmHttpConsoleGet       (int argCount, char **pArguments);


#endif /* _GSMHTTP_H_ */
