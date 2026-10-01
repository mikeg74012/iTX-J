/**
 **************************************************************************************************
 * @file   gsmComms.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Communication with the GSM modem.
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */
#ifndef GSMCOMMS_H_
#define GSMCOMMS_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* default timeout for most AT commands */
#define DEFAULT_AT_TIMEOUT_MS  300

/* AT Command Response Types */
typedef enum {
  AT_NONE,
  AT_IN_PROCESS,
  AT_COMPLETE,
  AT_READY_TO_SEND,
  AT_TIMEOUT,
  AT_ERROR,
} eAtResult;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
void      gsmCommsInitialiseModem (void);
void      GsmCommsTask            (void const * argument);
eAtResult gsmCommsSendAtCommand   (char * atCommand, int timeoutMs);
eAtResult gsmCommsSendString      (char *msg, int timeoutMs);
eAtResult gsmCommsSendData        (uint8_t * pData, int len, int timeoutMs);
eAtResult gsmCommsWaitForData     (int timeoutMs);
void      gsmCommsBinaryMode      (int binaryDataLength);


#endif /* GSMCOMMS_H_ */
