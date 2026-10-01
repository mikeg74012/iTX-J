/**
 **************************************************************************************************
 * @file   GsmUtils.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements the common GSM Utils functions
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#ifndef _GSMUTILS_H_
#define _GSMUTILS_H_

#include <stdbool.h>

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

#define GSM_REGISTER_TIMEOUT_SECS            (10)



/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmUtilsNetworkCheck                   (void);
bool GsmUtilsRequestUnsolicitedInfo         (void);
bool GsmUtilsNetworkInfoGet                 (int timeoutDuration);
bool GsmUtilsHandleConnectionRetry          (bool (*checkFn)(), const char* stageName, int duration);

#endif /* _GSMUTILS_H_ */
