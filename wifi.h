/**
 **************************************************************************************************
 * @file   wifi.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   04/05/2020
 *
 * @brief  Wifi Driver header file
 *
 **************************************************************************************************
 */

#ifndef WIFI_H
#define WIFI_H

#include "main.h"
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
typedef struct
{
  char    MacAddress[13];
  uint8_t Rssi;
} WifiMac_t;

/* Number of scan results stored */
#define WIFI_NO_OF_RESULTS          5

/* Duration of the WiFi scan */
#define WIFI_SCAN_DURATION_SECS       10


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void WifiTask           (void const * argument);
void WifiProgram        (void);
void WifiStart          (void);
void WifiStop           (void);
void WifiAbort          (void);
bool WifiMemCEFix       (void);
bool WifiGetScanResult  (WifiMac_t * pScanResult, int index);

/* Console Commands */
int WifiConsoleProg  (int argCount, char **pArguments);
int WifiConsoleStart (int argCount, char **pArguments);
int WifiConsoleStop  (int argCount, char **pArguments);
int WifiConsoleFix   (int argCount, char **pArguments);
int WifiConsoleList  (int argCount, char **pArguments);

#endif /* WIFI_H */
