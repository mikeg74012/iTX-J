/**
 **************************************************************************************************
 * @file   GsmUtils.c
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

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
static bool GsmUtilsNetworkInfoCheck               (void);

/*
 **************************************************************************************************
 *                                      PRIVATE VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern bool RegisteredOnNetwork;  /* flag indicates when device is registered on network */
extern bool GSMOperatorInfoGot;   /* flag to tell if the operator ID is got */

/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */


 /**
 * @brief   Get the Network Info with timeout
 * @param   duration  The total duration (in seconds) for retries attemps, with a delay of 1 second between each attempt. 
 * @return  bool      true if network info got, otherwise false
 */
bool GsmUtilsNetworkInfoGet (int timeoutDuration)
{
  bool result;
  result = GsmUtilsHandleConnectionRetry(GsmUtilsNetworkCheck, "GsmUtilsNetworkCheck", timeoutDuration);

  /* Confirm the GSMNetwork Info */
  if (result)
  {
    result = GsmUtilsHandleConnectionRetry(GsmUtilsNetworkInfoCheck, "GsmUtilsNetworkInfoCheck", timeoutDuration);
  }
  return result;
}


/**
 * @brief   Handles retries for connection attempts, with the checkFn provided
 * @param   checkFn   The function pointer to the specific check function for the stage (e.g., 
 *                      GsmUtilsNetworkCheck, GsmGprsNetworkCheck, etc.) that needs to be retried.
 * @param  stageName A string representing the name of the current stage being checked (used for logging/debugging).
 * @param  duration  The total duration (in seconds) for retries attemps, with a delay of 1 second between each attempt.
 *
 * @return    bool      true if the check function succeeds within the given retry period, false if it fails.
 *                      
 */
bool GsmUtilsHandleConnectionRetry(bool (*checkFn)(), const char* stageName, int duration)
{
  bool stageResult = false;

  /* Validate the parameters */
  if ((checkFn == NULL) || (stageName == NULL))
  {
    return false;
  }
  
  for (int timeout = 0; timeout < duration; timeout++)
  {
    stageResult = checkFn();
    if (stageResult || isModemAbort())
    {
      break;
    }
    sleep(ONE_SECOND_IN_MS);
  }

  /* Stage function failed, log the failure count */
  if (!stageResult)
  {
    ConsolePrintf("Failed at stage: %s with duration %d\r\n", stageName, duration);
  }
  
  return stageResult;
}


/**
 * @brief     check for GSM registration
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
bool GsmUtilsNetworkCheck (void)
{
  eAtResult AtResult;

  /* clear the registered flag */
  RegisteredOnNetwork = false;

  /* request network registration status */
  AtResult = gsmCommsSendAtCommand("AT+CREG?", DEFAULT_AT_TIMEOUT_MS);

  /* Check the result */
  if ((AT_COMPLETE == AtResult) && (true == RegisteredOnNetwork))
  {
    return true;
  }

  return (false);
}


/**
 * @brief     Request unsolicited network information from the modem
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
bool GsmUtilsRequestUnsolicitedInfo (void)
{
  eAtResult AtResult;

  /* enable network registration and location information status */
  AtResult = gsmCommsSendAtCommand("AT+CREG=2", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

 /**
 * @brief     Query the GSM Network Info from Modem 
 * @param[in] none
 * @return    status  : true on success, false on failure
 */
static bool GsmUtilsNetworkInfoCheck (void)
{
  eAtResult AtResult;

  /* clear the registered flag */
  GSMOperatorInfoGot = false;

  /* request network registration status */
  AtResult = gsmCommsSendAtCommand("AT+QENG?", DEFAULT_AT_TIMEOUT_MS);

  /* Check the result */
  if ((AT_COMPLETE == AtResult) && (true == GSMOperatorInfoGot))
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
