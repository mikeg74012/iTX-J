/**
 **************************************************************************************************
 * @file   main.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Main application for GD32
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

//#include "FreeRTOS.h"
//#include "timers.h"
//#include "bsp.h"
//#include "csp.h"
//#include "hardwareVersion.h"


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Debug Trace Enable for each module.  Set to 1 to enable, 0 to disable */
#ifdef DEBUG

#define TRACE_GSMCOMMS_ENABLE     1
#define TRACE_GNSSCOMMS_ENABLE    1
#define TRACE_MODEM_ENABLE        1
#define TRACE_SEQUENCER_ENABLE    1
#define TRACE_GNSS_ENABLE         1
#define TRACE_BATMON_ENABLE       1
#define TRACE_CHGR_ENABLE         1
#define TRACE_WATCHDOG_ENABLE     1
#define TRACE_IMU_ENABLE          1
#define TRACE_POWER_ENABLE        1
#define TRACE_POST_ENABLE         1
#define TRACE_CONFIG_ENABLE       1
#define TRACE_MESSAGES_ENABLE     1
#define TRACE_OTA_ENABLE          1
#define TRACE_WIFI_ENABLE         1

#else

#define TRACE_GSMCOMMS_ENABLE     1
#define TRACE_GNSSCOMMS_ENABLE    0
#define TRACE_MODEM_ENABLE        0     /* Enabling modem trace may occasionally cause GNSS char loss. Tracing with ITS-131. */
#define TRACE_SEQUENCER_ENABLE    1
#define TRACE_GNSS_ENABLE         0
#define TRACE_BATMON_ENABLE       1
#define TRACE_CHGR_ENABLE         1
#define TRACE_WATCHDOG_ENABLE     0
#define TRACE_IMU_ENABLE          1
#define TRACE_POWER_ENABLE        0
#define TRACE_POST_ENABLE         1
#define TRACE_CONFIG_ENABLE       1
#define TRACE_MESSAGES_ENABLE     0
#define TRACE_OTA_ENABLE          1
#define TRACE_WIFI_ENABLE         1

#endif


/* Modem Options */
#define MODEM_SEPARATE_GNSS_UART  1

/* OTA Options - battery threshold for OTA to be performed */
#define OTA_BATTERY_MIN_PERCENT   25

/* One second time definition */
#define ONE_SECOND_IN_MS          1000


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* Interface */
void Error_Handler      (void);
void SystemClockInit    (void);
void resetPeripherals   (void);

/* Console Commands */
int  SystemConsoleStack (int ArgCount, char **pArguments);
int  SystemConsoleReset (int ArgCount, char **pArguments);

#endif /* __MAIN_H */

