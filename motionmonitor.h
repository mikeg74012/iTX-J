/**
 **************************************************************************************************
 * @file   motionmonitor.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Motion Monitor Thread main file.
 *
 **************************************************************************************************
 */
#ifndef MOTIONMONITOR_H
#define MOTIONMONITOR_H

#include <stdbool.h>
#include "motionbuffer.h"

#define MM_DETECT_BUFFER_ELEMENTS 50              // 50 elements = approx 0.5 seconds (used for event detection)



/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Debug trace for the Motion Monitor and sub-modules */
#define TRACE_IMU(...)
/*
#if TRACE_IMU_ENABLE
#define TRACE_IMU(...) TRACE_COLOUR_(FG_YELLOW, BG_DEFAULT, "IMU", __VA_ARGS__)
#else
  #define TRACE_IMU(...)
#endif
*/

/* Period at which the motion monitor runs */
#define MM_TIMER_PERIOD_MS            200

/* number of mS per sample (@ 100 Hz) */
#define MM_MS_PER_SAMPLE              10


/*
 **************************************************************************************************
 *                                         PUBLIC VARIABLES
 **************************************************************************************************
 */
extern ImuData_t DetectBuffer[MM_DETECT_BUFFER_ELEMENTS];
extern uint32_t noOfDetectBufferElementsRead;

typedef enum MM_State
{
  WAKEUP_POWERUP = 0,
  WAKEUP_RTC,
  WAKEUP_IMU,
  WAKEUP_USB,
  WAKEUP_REDBUTTON,
  IN_JOURNEY,
  ENDING_JOURNEY
} eMmState;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* Public API */
void     MotionMonitorTask                   (void const * argument);
void     MotionMonitorSleepAllowed           (bool allowed);
void     MotionMonitorHibernate              (void);
void     MotionMonitorGetRawAccelData        (AccelerationData_t *pAccelData);
void     MotionMonitorImpactSequenceFinished (void);
void     MotionMonitorEojSequenceFinished    (void);
void     MotionMonitorDhmSequenceFinished    (void);
void     MotionMonitorDstmSequenceFinished   (void);
eMmState MotionMonitorGetState               (void);
uint8_t  GetImpactMaxSpeed                   (void);
uint32_t GetImpactMaxMagnitude               (void);
uint32_t GetLastDHMTime                      (void);
void     SetLastDHMTime                      (uint32_t ts);
int      GetLastHPfilterIndex                (void);
void     RestoreLastHPfilterIndex            (int hpfIndex);
uint8_t * GetHPFilerBufferPointer            (void);
uint16_t GetHPFilerBufferSize                (void);
void     ImpactPeakMagnitudeUpdate           (ImuData_t *pBuffer);
void     ImpactPeakSpeedUpdate               (uint32_t timeStamp);


/* Console Commands */
int      MotionMonitorConsoleSpoof           (int argCount, char **pArguments);
int      MotionMonitorConsoleShowData        (int argCount, char **pArguments);
int      MotionMonitorConsoleAllowSleep      (int argCount, char **pArguments);
int      MotionMonitorConsoleHibernate       (int argCount, char **pArguments);
int      MotionMonitorConsoleStream          (int argCount, char **pArguments);
int      MotionMonitorConsoleDhmTime         (int argCount, char **pArguments);
int      MotionMonitorConsoleReadyToSleep    (int argCount, char **pArguments);
int      MotionMonitorConsoleFakeMiniEvent   (int argCount, char **pArguments);

#endif /* MOTIONMONITOR_H */
