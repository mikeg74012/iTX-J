/**
 **************************************************************************************************
 * @file   batterymonitor.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Battery Monitor Thread main file.
 *
 **************************************************************************************************
 */

#ifndef BATTERYMONITOR_H
#define BATTERYMONITOR_H

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
#define BATTERY_WARNING_THRESHOLD_PERCENT     20          //  20% - warn user (red LED flash)

#define BATTERY_FULL_CHARGE                   100
#define BATTERY_LOG_VOLTS(vv)                 ((vv/100) << 8)

typedef enum
{
  NOT_CHARGING,
  CHARGING_IN_PROCESS,
  CHARGING_IN_PROCESS_CAPACITY_FULL,
  CHARGING_COMPLETE,
  UNKOWN_CHARGING_STATE
} ChargingState_e;


typedef enum PowerState
{
  POWER_STATE_NORMAL,         // Normal operation
  POWER_STATE_LOW,            // Battery low, no journeys
  POWER_STATE_CRITICAL        // Battery critical, no journey or modem power up
} ePowerState_t;


/*
 **************************************************************************************************
 *                                       PUBLIC VARIABLES
 **************************************************************************************************
 */
extern bool watchdogResetDetected;
extern bool UsbWakeUp;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void            BatteryMonitorTask             (void const * argument);
uint8_t         BatteryMonitorGetLevel         (void);
ChargingState_e BatteryMonitorGetChargingState (void);
bool            BatteryMonitorGetUsbState      (void);
int16_t         BatteryMonitorGetTemperature   (void);
int16_t         BatteryMonitorGetVoltage       (void);
int16_t         BatteryMonitorGetCurrent       (void);
void            BatteryMonitorUsbPowerISR      (void);
ePowerState_t   BatteryMonitorGetPowerState    (void);
bool            BatteryGetChargerState         (void);
void            BatteryRestorePowerState       (int prevState);

/* Console commands */
int             BatteryMonitorConsoleStatus    (int argCount, char **pArguments);
int             BatteryMonitorConsoleLog       (int argCount, char **pArguments);
int             BatteryMonitorConsoleSync      (int argCount, char **pArguments);

#endif /* BATTERYMONITOR_H */
