/**
 **************************************************************************************************
 * @file   GsmVoice.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements a GSM Voice Call functions
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#ifndef _GSMVOICE_H_
#define _GSMVOICE_H_

#include <stdbool.h>

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

/* emergency call status in process */
typedef enum
{
  CS_NONE,                            /* No call initiated */
  CS_INITIALIZED,                     /* Call Initialized, GSM network found */
  CS_NO_ANSWERED,                     /* No one answer the call from the call centre */
  CS_CANCELED_BY_USER,                /* The call actively cancelled by user, e.g. Red Button */
  CS_TERMINATED_BY_TIMEOUT,           /* The call terminated by the call centre, because max duration reached*/
  CS_SUCCESS,                         /* Call centre handled the call */
  CS_FAILED,                          /* Call Initialized, but no network found at all */
  CS_DIAL_TIMEOUT,                    /* Call Initialized, Dial number timed out */
} eGSMCallStatus;



/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmVoiceCall        (char * pNumber);
bool GsmVoiceHangUp      (void);
bool GsmVoiceAbort       (void);
bool GsmStopPlayTone     (void);
bool GsmVoiceRequestHangUp (void);
unsigned int GsmVoiceGetCallDuration(void);
eGSMCallStatus GsmGetVoiceCallStatus(void);
void GsmVoiceStatusReset            (void);

/* Console Commands */
int GsmVoiceConsoleCall (int argCount, char **pArguments);
int GsmVoiceConsoleCallStatus (int argCount, char **pArguments);
int GsmVoiceConsoleCallDur (int argCount, char **pArguments);

#endif /* _GSMVOICE_H_ */
