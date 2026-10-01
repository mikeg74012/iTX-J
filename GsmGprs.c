/**
 **************************************************************************************************
 * @file   gsmGprs.c
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

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
#define GPRS_REGISTER_TIMEOUT_SECS   10
#define GPRS_REGISTER_TIMEOUT_EXTEND_SECS       (4*GPRS_REGISTER_TIMEOUT_SECS)
#define GPRS_ACT_TIMEOUT_MS          30000
#define GPRS_DEACT_TIMEOUT_MS        30000

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static char AtCommand[MAX_CONSOLE_AT_LENGTH];
static uint32_t gsmLastOnlineTime = 0;
static bool needExtraTimeout = false;
static bool networkPdpError = false;    /* flag to tell if the modem returned a PDP activation error instead of a simple timeout */


/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern bool RegisteredOnGPRS;


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool GsmGprsSetPdpContext        (int context);
static bool GsmGprsActivatePdpContext   (void);
static bool GsmGprsDeactivatePdpContext (void);
static bool GsmGprsConfigureAPN         (void);
static bool GsmGprsRegisterTcpIp        (void);
static bool GsmGprsQueryState           (void);
static bool GsmGprsNetworkCheck         (void);


/*
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS (API)
 **************************************************************************************************
 */
/**
 * @brief       Connect to the GPRS service
 * @param       context - the context of the PDP to connect
 * @return      true on success, false on failure
 */
bool GsmGprsConnect (int context, bool startTcpIp)
{
  bool result = false;
  NetworkConnError_t netError = NETWORK_NO_ERR;

  int timeoutDuration = GPRS_REGISTER_TIMEOUT_SECS;  // Initial timeout duration
  
  if (needExtraTimeout)
  {
    /* Extend timeout if requested */
    timeoutDuration = GPRS_REGISTER_TIMEOUT_EXTEND_SECS;
    
    /* Always reset the extra timeout to save power, need to enable for each time */
    needExtraTimeout = false;
  }

  /* set the foreground PDP context */
  result = GsmGprsSetPdpContext(context);

  /* set the APN for GPRS network access */
  if (result)
  {
    result = GsmGprsConfigureAPN();
    if (!result)
    {
      netError = NETWORK_APN_ERROR;
    }
  }

  /* request unsolicited network information from modem */
  if (result)
  {
    GsmUtilsRequestUnsolicitedInfo();
  }

  /*
   * Check we are registered on GSM
   * Per Quectel documentation, we retry once per second for 10 seconds, 
   * For the ITS-121, the timeout duration will be extended to 40 seconds once, after the third failed attempt.
   */
  if (result)
  {    
    result = GsmUtilsNetworkInfoGet(timeoutDuration);

    if (!result)
    {
      netError = NETWORK_GSM_REG_ERR;
    }
  }

  /*
   * Check we are registered on GPRS
   * Per Quectel documentation, we retry once per second for 10 seconds, 
   * For the ITS-121, the timeout duration will be extended to 40 seconds once, after the third failed attempt.
   */
  if (result)
  {
    result = GsmUtilsHandleConnectionRetry(GsmGprsNetworkCheck, "GsmGprsNetworkCheck", timeoutDuration);
    if (!result)
    {
      netError = NETWORK_GPRS_REG_ERR;
    }
  }

  if (result && startTcpIp)
  {
    ModemStatus.gprsIdle = false;
    result = GsmGprsQueryState();

    if (result && ModemStatus.gprsIdle)
    {
      /* Start TCP task if stack is idle */
      result = GsmUtilsHandleConnectionRetry(GsmGprsRegisterTcpIp, "GsmGprsRegisterTcpIp", timeoutDuration);
      if (!result)
      {
        netError = NETWORK_TCPIP_REG_ERR;
      }      
    }

    ModemStatus.gprsReg = false;
    result = GsmGprsQueryState();

    if (result && ModemStatus.gprsReg)
    {
      /* Activate the PDP context upon successful stack registration */
      /* Intentionally use the default GPRS_REGISTER_TIMEOUT_SECS for this retry, 
      * as the AT command for PDP activation, GPRS_ACT_TIMEOUT_MS, 
      * provides a sufficiently long timeout duration. */
      result = GsmUtilsHandleConnectionRetry(GsmGprsActivatePdpContext, "GsmGprsActivatePdpContext", GPRS_REGISTER_TIMEOUT_SECS);
      if (!result)
      {
        if (networkPdpError)
        {
          netError = NETWORK_PDP_ACTIVATE_ERR;
          /* acknowldge the error and reset the flag */
          networkPdpError = false;
        }
        else
        {
          netError = NETWORK_PDP_ACTIVATE_TIMEOUT;
        }
      }
    }

    /*
     * Note: Can still return success, even gprsIdle and gprsReg is false
     * if the GPRS/CSD context has been activated successfully before
     */

  }

  /* House keep to get the general RSSI for this connection process*/
  ModemGetRSSI();

  /* update the last success connection timestamp */
  if (startTcpIp && result)
  {
    gsmLastOnlineTime = RtcGetTimeStamp();
  }
  
  /* network error happened, log the event */
  if (netError != NETWORK_NO_ERR)
  {
    ModemLogNetworkError(netError);
  }
  return result;
}


/**
 * @brief       Disconnect from the GPRS service
 * @param       context - the context of the PDP to disconnect
 * @return      true on success, false on failure
 */
bool GsmGprsDisconnect (int context)
{
  bool result = false;

  result = GsmGprsDeactivatePdpContext();
  return result;
}


/**
 * @brief       set the extra timeout is needed for the GsmGprs Connection
 * @param       void
 * @return      void
 */
void GsmGprsSetExtraTimeout (void)
{
  needExtraTimeout = true;
}


/**
 * @brief       Get the Last GSM Connection timestamp
 * @param       none
 * @return      Last success GSM Connection timestamp for TCPIP connection
 */
uint32_t GsmGprsGetLastOnLineTime (void)
{
  return gsmLastOnlineTime;
}


/**
 * @brief       Restore the Last GSM Connection timestamp
 * @param       value - value from PSRAM restored from low power module
 * @return      none
 */
void LPRestoreGsmLastOnlineTime (uint32_t value)
{
  gsmLastOnlineTime = value;
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     Set the foreground PDP context
 * @param[in] context : The PDP context to swtich to
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsSetPdpContext (int context)
{
  eAtResult AtResult;

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QIFGCNT=%d", context);

  /* set the foreground PDP context */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Activate the current PDP context
 *            set the networkPdpError to modem return Rx-Error explicitly
 *            It is potentially due to authentication issues
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsActivatePdpContext (void)
{
  eAtResult AtResult;

  /* set the APN */
  AtResult = gsmCommsSendAtCommand("AT+QIACT", GPRS_ACT_TIMEOUT_MS);

  if (AtResult == AT_ERROR)
  {
    /* Error explicitly reported by the modem when QIACT, potentially due to authentication issues */
    networkPdpError = true;
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Deactivate the current PDP context
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsDeactivatePdpContext (void)
{
  eAtResult AtResult;

  /* set the APN */
  AtResult = gsmCommsSendAtCommand("AT+QIDEACT", GPRS_DEACT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Configure the Access Point details
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsConfigureAPN (void)
{
  eAtResult AtResult;

  /* Build the AT command */
  char * ApnUrl  = ConfigGetApnUrl();
  char * ApnUser = ConfigGetApnUsername();
  char * ApnPass = ConfigGetApnPassword();
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QICSGP=1,\"%s\",\"%s\",\"%s\"", ApnUrl, ApnUser, ApnPass);

  /* set the APN */
  AtResult = gsmCommsSendAtCommand(AtCommand, 500);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Register on the TCP/IP stack
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsRegisterTcpIp (void)
{
  eAtResult AtResult;

#if 0
  /* Start TCP/IP task */
  AtResult = gsmCommsSendAtCommand("AT+QIREGAPP", DEFAULT_AT_TIMEOUT_MS);
#else
  /* Build the AT command */
  char * ApnUrl  = ConfigGetApnUrl();
  char * ApnUser = ConfigGetApnUsername();
  char * ApnPass = ConfigGetApnPassword();
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QIREGAPP=\"%s\",\"%s\",\"%s\"", ApnUrl, ApnUser, ApnPass);

  /* Start TCP/IP task */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
#endif

  return ((AtResult == AT_COMPLETE) ? true : false);
}

/**
 * @brief     Query the TCP/IP Stack State
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsQueryState (void)
{
  eAtResult AtResult;

  /* Build the AT command */
  AtResult = gsmCommsSendAtCommand("AT+QISTAT", DEFAULT_AT_TIMEOUT_MS);

  /* STATE come AFTER the first "OK" */
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsWaitForData(DEFAULT_AT_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     check for GPRS registration
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmGprsNetworkCheck (void)
{
  eAtResult AtResult;

  /* clear the registered flag */
  RegisteredOnGPRS = false;

  /* request GPRS network registration status */
  AtResult = gsmCommsSendAtCommand("AT+CGREG?", DEFAULT_AT_TIMEOUT_MS);

  /* Check the result */
  if ((AT_COMPLETE == AtResult) && (true == RegisteredOnGPRS))
  {
    return true;
  }

  return (false);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Connect to the GPRS network
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGprsConsoleConnect (int argCount, char **pArguments)
{
  int context = GPRS_DEFAULT_PDP_CONTEXT;

  if (1 == argCount)
  {
    /* get the context */
    sscanf(pArguments[0],  "%d", &context);
  }

  GsmGprsConnect(context, true);
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Disconnect from the GPRS network
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGprsConsoleDisconnect (int argCount, char **pArguments)
{
  int context = GPRS_DEFAULT_PDP_CONTEXT;

  if (1 == argCount)
  {
    /* get the context */
    sscanf(pArguments[0],  "%d", &context);
  }

  GsmGprsDisconnect(context);
  return (CONSOLE_ERROR_OK);
}

