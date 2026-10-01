/**
 ******************************************************************************
 * @file   configupdate.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   24/03/2020
 *
 * @brief  Handles the update of Config Data
 *
 ******************************************************************************
 */

#ifndef CONFIG_UPDATE_H
#define CONFIG_UPDATE_H

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Types of config message block */
typedef enum SmsConfigType
{
  TERMINATOR_TYPE     = 0,        // Used to terminate the SMS message
  OUTGOING_SMS_TYPE   = 1,        // Set the outgoing SMS number
  VOICECALL_NO_TYPE   = 2,        // Set the outgoing voice number
  GPRS_URL_TYPE       = 3,        // Set the URL to use for reporting
  IP_ADDRESS_TYPE     = 4,        // Set the IP address for raw TCP / UDP reporting
  IP_PORT_TYPE        = 5,        // Set the IP port for raw TCP / UDP reporting
  FW_UPDATE_URL_TYPE  = 6,        // Set the URL to fetch firmware update images
  HIBERNATE_REQ_TYPE  = 7,        // Hibernate the device
  FLAG_TYPE           = 8,        // Set the value of a flag
  CUSTOMER_TYPE       = 9,        // Set the customer value
  PARAMETER_TYPE      = 10,       // Set the value of a parameter
  RESET_REQ_TYPE      = 11,       // Reset the device immediately
  APN_URL_TYPE        = 12,       // Set the APN URL
  APN_USERNAME_TYPE   = 13,       // Set the APN Username
  APN_PASSWORD_TYPE   = 14,       // Set the APN Password
  CONFIG_MAX_TLVS
} eSmsConfigType;


/*
 ******************************************************************************
 *                  PUBLIC FUNCTION PROTOTYPES / INTERFACE                    *
 ******************************************************************************
 */
bool ConfigUpdateConfigFromSms (char *smsString);
bool ConfigTextGetValue (uint8_t blockId, uint8_t blockIndex, char *pBuf, uint8_t bufLen);


#endif /* CONFIG_UPDATE_H */
