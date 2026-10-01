/**
 **************************************************************************************************
 * @file   GsmVoice.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements GSM Voice Call functions
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
/* maximum time for call to be answered */
#define VOICE_MAX_CONNECT_TIME_MS            (60 * 1000)

/* maximum duration for a call in process */
#define VOICE_MAX_CALL_TIME_MS               (60 * 60 * 1000)


/*
 **************************************************************************************************
 *                                      PRIVATE VARIABLES
 **************************************************************************************************
 */
static char AtCommand[MAX_CONSOLE_AT_LENGTH];
static bool RequestCallHangUp = false;
static unsigned int CallDurationInMs= 0;
static eGSMCallStatus GSMVoiceCallStatus = CS_NONE;


/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern bool callConnected;        /* flag indicates when a call is connected */


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool GsmVoiceDialNumber     (char * pNumber);
static bool GsmVoiceWaitConnect    (unsigned int timeoutMs);
static bool GsmVoiceWaitDisconnect (unsigned int timeoutMs);




/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief  Make a voice call to the number provided
 * @param  pNumber : The phone number to call, as a string
 * @retval status  : true on success, false on failure
 */
bool GsmVoiceCall (char * pNumber)
{
  RequestCallHangUp = false;
  CallDurationInMs= 0;
  callConnected = false;

  /* Make sure we are connected to the network, enable the unsolicited Info for checking */
  bool result = GsmUtilsRequestUnsolicitedInfo();

  if (result)
  {
    result = GsmUtilsHandleConnectionRetry(GsmUtilsNetworkCheck, "GsmUtilsNetworkCheck", GSM_REGISTER_TIMEOUT_SECS);
  }

  /* dial the number */
  if (result)
  {
    GSMVoiceCallStatus = CS_INITIALIZED;
    result = GsmVoiceDialNumber(pNumber);
  }
  else
  {
    GSMVoiceCallStatus = CS_FAILED;
  }
  /* Wait for the call to connect */
  if (result)
  {
    result = GsmVoiceWaitConnect(VOICE_MAX_CONNECT_TIME_MS);
  }
  else
  {
    /* deal with Dial time out*/
    GSMVoiceCallStatus = CS_DIAL_TIMEOUT;
    TRACE_MODEM("Dial timed out \n\r");
    GsmVoiceHangUp();
    return result;
  }

  /* the number dialed OK, Process continues*/
  if (!result)
  {
    /* deal with timeout (no answer).  Hang up */
    GsmVoiceHangUp();
    GSMVoiceCallStatus = CS_NO_ANSWERED;
  }
  else
  {
    /* wait for remote end to hang up */
    if (false == GsmVoiceWaitDisconnect(VOICE_MAX_CALL_TIME_MS))
    {
      /* call went on too long.  Hang up */
      TRACE_MODEM("Call too long - We hung up\n\r");
      GsmVoiceHangUp();
      GSMVoiceCallStatus = CS_TERMINATED_BY_TIMEOUT;
    }
    else
    {
      if (RequestCallHangUp)
      {
        GSMVoiceCallStatus = CS_CANCELED_BY_USER;
      }
      else
      {
        /* call are all good */
        GSMVoiceCallStatus = CS_SUCCESS;
      }
    }
  }

  return result;
}


/**
 * @brief  Terminate the ongoing call
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmVoiceHangUp (void)
{
  TRACE_MODEM("Hanging up\n\r");

  eAtResult AtResult = gsmCommsSendAtCommand("ATH0", 3000);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Get the current call durations
 * @param  None
 * @retval current call duration in ms
 */
unsigned int GsmVoiceGetCallDuration (void)
{
  TRACE_MODEM("Current call duration: %d\n\r", CallDurationInMs);

  return CallDurationInMs;
}

/**
 * @brief  Get the current call status
 * @param  None
 * @retval GSMVoiceCallStatus the call status of last GSM
 */
eGSMCallStatus GsmGetVoiceCallStatus (void)
{
  TRACE_MODEM("Current call status: %d\n\r", GSMVoiceCallStatus);

  return GSMVoiceCallStatus;
}


/**
 * @brief  Request an ongoing call to be terminated
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmVoiceRequestHangUp (void)
{
  TRACE_MODEM("Requesting call hang up\n\r");
  RequestCallHangUp = true;

  return true;
}


/**
 * @brief  Request the status of call(s) in process
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmVoiceGetCallStatus (void)
{
  eAtResult AtResult = gsmCommsSendAtCommand("AT+CLCC", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Abort voice call
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmVoiceAbort (void)
{
  return true;
}


/**
 * @brief  reset voice call status and duration
 * @param  None
 * @retval none
 */
void GsmVoiceStatusReset (void)
{
  CallDurationInMs= 0;
  GSMVoiceCallStatus = CS_NONE;
}

/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */


/**
 * @brief  Dial the number provided
 * @param  pNumber : The phone number to call, as a string
 * @retval status  : true on success, false on failure
 */
static bool GsmVoiceDialNumber (char * pNumber)
{
  eAtResult AtResult;

  TRACE_MODEM("Dialling Number %16s\n\r", pNumber);

  /* Set up the AT command to dial the requested number Dial Number command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "ATD%s;", pNumber);

  /* send command to the modem to start dialling */
  AtResult = gsmCommsSendAtCommand(AtCommand, 1000);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Wait for a call to connect
 * @param  timeoutMs : Maximum time to wait for call to be answered
 * @retval status    : true on success, false on failure
 */
static bool GsmVoiceWaitConnect (unsigned int timeoutMs)
{
  eAtResult AtResult;
  int WaitTime = 0;

  /* check call status every 500 mS until timeoutMs expires */
  while (WaitTime < timeoutMs)
  {
    sleep(500);
    WaitTime += 500;

    WatchdogSetActive(GSMMANAGERTASK);

    if (RequestCallHangUp)
    {
      GsmVoiceHangUp();
      return true;
    }

    /* request call status */
    AtResult = gsmCommsSendAtCommand("AT+CLCC", 500);

    /* check if the call has been answered */
    if ((AT_COMPLETE == AtResult) && (callConnected))
    {
      TRACE_MODEM("Call Connected\n\r");
      return true;
    }
  }

  TRACE_MODEM("No answer\n\r");
  return false;
}


/**
 * @brief  Wait while the call is active (until the other party hangs up)
 * @param  timeoutMs : Maximum time to wait for call to be disconnected
 * @retval status    : true on success, false on failure
 */
static bool GsmVoiceWaitDisconnect (unsigned int timeoutMs)
{
  unsigned int WaitTime = 0;

  /* check call status every 500 mS until timeoutMs expires */
  while (WaitTime < timeoutMs)
  {
    sleep(500);
    WaitTime += 500;
    CallDurationInMs= WaitTime;

    WatchdogSetActive(GSMMANAGERTASK);

    if (RequestCallHangUp)
    {
      GsmVoiceHangUp();
      return true;
    }

    if (!callConnected)
    {
      TRACE_MODEM("Call Disconnected\n\r");
      return true;
    }
  }
  return false;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Make a MO voice call
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmVoiceConsoleCall (int argCount, char **pArguments)
{
  switch (argCount)
  {
    case 0:
      /* use the phone number from the config store */
      ModemMakeVoiceCall(ConfigGetVoiceNumber());
      break;
    case 1:
      /* use the phone number from the command line */
      ModemMakeVoiceCall(pArguments[0]);
      break;
    default:
      return (CONSOLE_ARG_NO);
  }
  return (CONSOLE_ERROR_OK);
}

/**
 * @brief       Get the current GSM Call Status
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GsmVoiceConsoleCallStatus (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* last GSM Call status */
    GsmGetVoiceCallStatus();

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}

/**
 * @brief       Get the current GSM call durations
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GsmVoiceConsoleCallDur (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* last GSM Call Duration */
    GsmVoiceGetCallDuration();

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}
