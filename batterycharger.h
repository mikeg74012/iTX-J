/**
 **************************************************************************************************
 * @file   batterycharger.h
 **************************************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   01/04/2020
 *
 * @brief  Battery Charger IC Driver header file
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */

#ifndef BATTERYCHARGER_H
#define BATTERYCHARGER_H

#include <stdbool.h>
#include "main.h"
#include "i2c.h"


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */

#define BATTERCHARGER_BQ24295_PN_MASK  0xC0 // Vender identifier reported by Revision Status Register TI BQ24295 [Bit5:7]
#define BATTERCHARGER_BQ24292_PN_MASK  0x18 // Vender identifier reported by Revision Status Register TI BQ24292I[Bit3:5]

#define BATTERYCHARGER_ADDRESS        (0x6B << 1)  /* BQ24295 I2C 8-bit address byte */
#define NUMOFRETRIES 2u

//#define I2C_COMMS_TIMEOUT     HAL_MAX_DELAY//5000u

/* Battery Charger registers --------------------------------------------*/

#define BATTERYCHARGER_REG00          0x00 /* Input Source Control Register REG00 [reset = 01011000, or 0x58]*/
#define BATTERYCHARGER_REG01          0x01 /* Power-On Configuration Register REG01 [reset = 00111011, or 0x3B]*/
#define BATTERYCHARGER_REG02          0x02 /* Charge Current Control Register REG02 [reset = 00100000, or 0x20]*/
#define BATTERYCHARGER_REG03          0x03 /* Pre-Charge/Termination Current Control Register REG03 [reset = 00010001, or 0x11]*/
#define BATTERYCHARGER_REG04          0x04 /* Charge Voltage Control Register REG04 [reset = 10110010, or 0xB2]*/
#define BATTERYCHARGER_REG05          0x05 /* Charge Termination/Timer Control Register REG05 [reset = 10011100, or 0x9C]*/
#define BATTERYCHARGER_REG06          0x06 /* Boost Voltage/Thermal Regulation Control Register REG06 [reset = 10010011, or 0x93]*/
#define BATTERYCHARGER_REG07          0x07 /* Misc Operation Control Register REG07 [reset = 01001011, or 0x4B]*/
#define BATTERYCHARGER_REG08          0x08 /* System Status Register REG08*/
#define BATTERYCHARGER_REG09          0x09 /* New Fault Register REG09*/
#define BATTERYCHARGER_REG0A          0x0A /* Vender / Part / Revision Status Register REG0A */

/* Battery Register Mask ------------------------------------------------*/
#define REG08_CHRG_STAT_MASK          0x30
#define REG08_CHRG_STAT_FULLY_CHARGE  0x30
#define REG08_CHRG_STAT_NOT_CHARGING  0x00

#define REG08_PG_STAT_MASK_POS        2
#define REG08_PG_STAT_MASK            (1 << REG08_PG_STAT_MASK_POS) /* Register 08: Power Good Bit (0 = Not Power Good, 1 = Power Good) */

#define BATTERY_CHARGER_DCP_PORT      0x01       /* for device with BQ24292 type of charger control */
#define BATTERY_CHARGER_BQ24292       0x02
#define BATTERY_CHARGER_BQ24295       0x04

/* Battery Charger variables definition ------------------------------------*/

/* Charger status report. */
typedef enum
{
  BATTERYCHARGER_OK = 0u,
  BATTERYCHARGER_ERROR,
  BATTERYCHARGER_ERROR_BUFFER,
  BATTERYCHARGER_COMMS_TIMEOUT,
  BATTERYCHARGER_DATANOTREADY,
  BATTERYCHARGER_UNKNOWN,
  BATTERYCHARGER_ENABLED,
  BATTERYCHARGER_DISABLED,
  BATTERYCHARGER_NO_EXTERNAL_POWER,
  BATTERYCHARGER_NOT_CHARGING,
  BATTERYCHARGER_FAST_CHARGE,
  BATTERYCHARGER_COMPLETE,
} ChargerStatus_t;

/* Battery Charger faults to match REG09 output definitions. */
typedef enum
{
  BATTERYCHARGER_FAULT_NONE = 0x00,
  BATTERYCHARGER_FAULT_THERMISTOR_TOO_HOT = 0x01,
  BATTERYCHARGER_FAULT_THERMISTOR_TOO_COLD = 0x02,
  // Value 0x04 is reserved
  BATTERYCHARGER_FAULT_BATTERY_OVER_VOLTAGE = 0x08,
  BATTERYCHARGER_FAULT_INPUT_FAULT = 0x10, //!< Note that the value of BATTERYCHARGER_FAULT_CHARGE_TIMER_EXPIRED overlaps this, be careful when testing the bitmap.
  BATTERYCHARGER_FAULT_THERMAL_SHUTDOWN = 0x20, //!< Note that the value of BATTERYCHARGER_FAULT_CHARGE_TIMER_EXPIRED overlaps this, be careful when testing the bitmap.
  BATTERYCHARGER_FAULT_CHARGE_TIMER_EXPIRED = 0x30, //!< This looks odd as it overlaps the two above but it matches the register meaning as defined by the chip.
  BATTERYCHARGER_FAULT_OTG = 0x40,
  BATTERYCHARGER_FAULT_WATCHDOG_EXPIRED = 0x80,
} ChargerFault_t;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
ChargerStatus_t ChargerInit                 ();
ChargerStatus_t ChargerLPSleepRestore       ();
ChargerStatus_t ChargerEnableCharging       (void);
ChargerStatus_t ChargerDisableCharging      (void);
ChargerStatus_t ChargerIsChargingEnabled    (void);
ChargerStatus_t ChargerEnterShippingMode    (void);
ChargerStatus_t ChargerSetInputCurrentLimit (void);
bool            ChargerIsBQ24292            (void);
bool            ChargerIsFullyCharged       (void);
bool            ChargerIsChargingInProgress (void);
bool            ChargerWUST                 (void);
uint8_t         ChargeStatusFlag            (void);
int             ChargerConsoleStatus        (int argCount, char **pArguments);
int             ChargerConsoleEnable        (int argCount, char **pArguments);
int             ChargerConsoleDisable       (int argCount, char **pArguments);
int             ChargerConsolePeek          (int argCount, char **pArguments);
int             ChargerConsolePoke          (int argCount, char **pArguments);

#endif /* BATTERYCHARGER_H */
