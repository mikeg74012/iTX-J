/**
 **************************************************************************************************
 * @file   watchdog.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2022/02/25
 *
 * @brief  Watchdog module with monitoring of individual RTOS tasks.
 *
 *         IMPORTANT NOTES
 *         1. This module does not use the hardware watchdog on the STM32.  This is because the two
 *            available hardware watchdog timers are unsuitable for some applications:
 *            The IWDG timer does not stop during MCU_Stop or MCU_Standby so incompatible with sleep.
 *            The WWDG timer has a very short timeout (58 mS max) which is too short for us to be
 *            able to program the on-chip FLASH reliably.  A mass erase takes roughly 50 mS.
 *         2. Instead we use an RTOS timer to create a software watchdog.  Although not as secure
 *            as a hardware watchdog, it is sufficient for non safety-critical applications.
 *
 **************************************************************************************************
 * @attention                     (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the Watchdog */
#if TRACE_WATCHDOG_ENABLE
  #define TRACE_WATCHDOG(...) TRACE_("WATCHDOG", __VA_ARGS__)
#else
  #define TRACE_WATCHDOG(...)
#endif

/* structure to hold information about each RTOS task */
typedef struct WdTask
{
  uint32_t TimeoutLengthMs;
  uint32_t ElapsedTimeMs;
  bool Waiting;
} WatchdogTask_t;

/* WatchDog status checks every 40 milliseconds */
#define WATCHDOG_KICK_PERIOD_MS         40

/* Period we wait to hear back from tasks before we pull the plug */
#define WATCHDOG_DEFAULT_TIMEOUT_MS     20000    // 20 seconds


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* Timer handle */
static TimerHandle_t WatchdogTimer;

/* Watchdog state for each individual RTOS task */
static WatchdogTask_t WatchdogTask[WatchDogTaskIDMAX];


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void WatchDogTimerCallback  (xTimerHandle timer);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Initialise watchdog.
 *            NOTE: Does NOT start the watchdog
 * @param     (none)
 * @return    (none)
 */
void WatchdogInitialize (void)
{
  /* initialize the individual task settings to default values */
  for (int TaskId = 0; TaskId < WatchDogTaskIDMAX; TaskId++)
  {
    WatchdogTask[TaskId].TimeoutLengthMs = WATCHDOG_DEFAULT_TIMEOUT_MS;
    WatchdogTask[TaskId].ElapsedTimeMs = 0;
    WatchdogTask[TaskId].Waiting = true;
  }
}


/**
 * @brief     Start the watchdog.  Should happen when RTOS tasking starts up
 * @param     none
 * @return    (none)
 */
void WatchdogStart (void)
{
  /* create a timer to keep tabs on the individual RTOS tasks */
  WatchdogTimer = xTimerCreate("WatchDogMonitorTimer", WATCHDOG_KICK_PERIOD_MS, pdTRUE, (void*) NULL, WatchDogTimerCallback);
  if (WatchdogTimer == NULL)
  {
    TRACE_WATCHDOG("ERROR: WatchDog Timer not created\n\r");
  }
  xTimerStart(WatchdogTimer, portMAX_DELAY);
}


/**
 * @brief     Set Timeout length for a specific RTOS task
 * @param     (eWatchDogTaskID_t watchDogTaskID to set alive)
 * @return    (none)
 */
void WatchdogSetTimeoutLength  (eWatchDogTaskID_t watchDogTaskID, uint32_t timeoutMs)
{
  WatchdogTask[watchDogTaskID].TimeoutLengthMs = timeoutMs;
}


/**
 * @brief     Set status to indicate that a Task is waiting for some event (e.g. button press or incoming data)
 * @param     (eWatchDogTaskID_t watchDogTaskID to set alive)
 * @return    (none)
 */
void WatchdogSetWaiting (eWatchDogTaskID_t watchDogTaskID)
{
  WatchdogTask[watchDogTaskID].Waiting = true;
  WatchdogTask[watchDogTaskID].ElapsedTimeMs = 0;
}


/**
 * @brief     Set status to indicate that a Task is active / running
 * @param     (eWatchDogTaskID_t watchDogTaskID to set alive)
 * @return    (none)
 */
void WatchdogSetActive (eWatchDogTaskID_t watchDogTaskID)
{
  WatchdogTask[watchDogTaskID].Waiting = false;
  WatchdogTask[watchDogTaskID].ElapsedTimeMs = 0;
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     The Watch Dog Task Callback function
 * @param     xTimerHandle timer
 * @return    None
 */
void WatchDogTimerCallback (xTimerHandle timer)
{
  bool WatchdogTimeout = false;

  /* check each task to see if is OK */
  for (int TaskId = 0; TaskId < WatchDogTaskIDMAX; TaskId++)
  {
    if (!WatchdogTask[TaskId].Waiting)
    {
      WatchdogTask[TaskId].ElapsedTimeMs += WATCHDOG_KICK_PERIOD_MS;

      if (WatchdogTask[TaskId].ElapsedTimeMs > WatchdogTask[TaskId].TimeoutLengthMs)
      {
        WatchdogTimeout = true;
        ConsolePrintf("Watchdog Timeout: Task %d\n\r", TaskId);
      }
      else if (WatchdogTask[TaskId].ElapsedTimeMs > (WatchdogTask[TaskId].TimeoutLengthMs / 2))
      {
        ConsolePrintf("Watchdog Warning: Task %d\n\r", TaskId);
      }
    }
  }

  /* if a thread has failed to check in, reset the MCU */
  if (WatchdogTimeout)
  {
    PowerResetMCU();
  }
}



