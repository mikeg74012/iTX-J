/*
 **************************************************************************************************
 * @file   detectevent.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module scans the accelerometer ringbuffer for various event types, such as
 *         start of journey, end of journey, impact and mini-event
 *
 **************************************************************************************************
 */

#ifndef _DETECTEVENT_H_
#define _DETECTEVENT_H_


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * Default threshold values for detection of IMU events
 * NOTE: these may be overridden by values from the config store
 */
#define SOJ_DETECT_THRESHOLD_XYZ         31                 // Threshold in mG
#define EOJ_DETECT_THRESHOLD_XYZ         15                 // Threshold in mG
#define SOJ_DETECT_TIMEOUT_MS            ((10 * 1000))      // 10 seconds
#define EOJ_DETECT_TIMEOUT_MS            ((2 * 60 * 1000))  // 2 minutes

#define MM_MINIEV_DETECT_THRESHOLD       2000               // Threshold in mG
#define MM_IMPACT_DETECT_THRESHOLD       3500               // Threshold in mG
#define MM_MINIEV_MIN_DURATION_MS        0                  // minimum impulse duration in mS
#define MM_IMPACT_MIN_DURATION_MS        50                 // minimum impulse duration in mS
#define MM_IMPACT_MIN_SPEED_KPH          50                 // minimum vehicle speed for impact detection (speed from GNSS, in unit of kph*10)

#define MM_CORNER_DETECT_THRESHOLD_Y     300                // Threshold for corner detection in mG
#define MM_CORNER_MIN_DURATION_MS        50                 // minimum corner duration in mS
#define MM_CORNER_MIN_SPEED_KPH          320                // minimum vehicle corner speed (speed from GNSS x 0.1=Kilometre per hour, about 20mph)


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool DetectEventSOJ              (uint16_t noOfSamples);
bool DetectEventEOJ              (uint16_t noOfSamples);
bool DetectEventImpact           (uint16_t noOfSamples);
bool DetectEventMini             (uint16_t noOfSamples);
bool DetectEventCorner           (uint16_t noOfSamples);
bool DetectEventSojInProcess     (void);
bool DetectEventImpactInProcess  (void);
bool DetectFreefallEvent         (void);
uint32_t DetectEventGetImpactForceArea (void);

/* Console Commands */
int  DetectEventConsoleCalcs  (int argCount, char **pArguments);


#endif /* _DETECTEVENT_H_ */
