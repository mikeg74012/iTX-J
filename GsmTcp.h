/**
 **************************************************************************************************
 * @file   gsmTcp.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements raw TCP functionality for GSM Modems.
 *
 * @notes: Only one Tcp context is currently provided.  This module could be modified to provide
 *         multiple contexts in the future
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */
#ifndef _GSMTCP_H_
#define _GSMTCP_H_

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
bool GsmTcpConfigAndSend  (uint8_t * pData, uint32_t size);
bool GsmTcpConfigure      (void);
bool GsmTcpSend           (uint8_t * pData, uint32_t size);
bool GsmTcpClose          (void);
bool GsmTcpFlush      (void);

/* Console Command */
int      GsmConfigConsoleTCP     (int argCount, char **pArguments);
#endif /* _GSMTCP_H_ */
