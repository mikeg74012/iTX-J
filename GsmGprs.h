/**
 **************************************************************************************************
 * @file   gsmGprs.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements GPRS functionality for GSM Modems.
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#ifndef _GSMGPRS_H_
#define _GSMGPRS_H_

#include <stdbool.h>

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Default PDP context to use for GPRS data */
#define GPRS_DEFAULT_PDP_CONTEXT  0


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* Connect to the GPRS network */
bool GsmGprsConnect          (int context, bool startTcpIp);
void GsmGprsSetExtraTimeout  (void);

/* Disconnect from the GPRS network */
bool                GsmGprsDisconnect           (int context);
uint32_t            GsmGprsGetLastOnLineTime    (void);
void                LPRestoreGsmLastOnlineTime  (uint32_t value);

/* Console Commands */
int GsmGprsConsoleConnect    (int argCount, char **pArguments);
int GsmGprsConsoleDisconnect (int argCount, char **pArguments);


#endif /* _GSMGPRS_H_ */
