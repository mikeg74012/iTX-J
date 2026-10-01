/**
 **************************************************************************************************
 * @file   watchdog.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2022/02/25
 *
 * @brief  Watchdog module with monitoring of individual RTOS tasks
 *
 **************************************************************************************************
 * @attention           (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
typedef enum WatchDogTaskIDs
{
  CONSOLETASK        = 0,
  BUTTONMONITORTASK  = 1,
  BATTERYMONITORTASK = 2,
  MOTIONMONITORTASK  = 3,
  GSMMANAGERTASK     = 4,
  GSMCOMMSTASK       = 5,
  WatchDogTaskIDMAX
} eWatchDogTaskID_t;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */

void WatchdogInitialize        (void);
void WatchdogStart             (void);
void WatchdogSetTimeoutLength  (eWatchDogTaskID_t watchDogTaskID, uint32_t timeoutMs);
void WatchdogSetWaiting        (eWatchDogTaskID_t watchDogTaskID);
void WatchdogSetActive         (eWatchDogTaskID_t watchDogTaskID);

#endif /* WATCHDOG_H */
