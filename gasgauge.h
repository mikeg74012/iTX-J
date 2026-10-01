/**
 ******************************************************************************
 * @file   gasgauge.h
 ******************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   25/03/2020
 *
 * @brief  Gas Gauge Driver header
 *
 ******************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */

#ifndef GASGAUGE_H
#define GASGAUGE_H

#include <stdbool.h>
#include "main.h"


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */

#define GASGAUGE_SENSERESISTOR      30.0f    // sense resistor value in milliOhms (10min, 100max)

#define GASGAUGE_ADDRESS_8BIT       0xE0u   /* STC3100 8-bit address byte */
#define GASGAUGE_ADDRESS_7BIT       0x70u   /* STC3100 7-bit address byte */
#define NUMOFRETRIES 2u

#define I2C_COMMS_TIMEOUT     HAL_MAX_DELAY//5000u

/*Address of the GAS GAUGE register --------------------------------------------*/

#define GASGAUGE_REG_MODE                 0x00 /* Mode Register                 */
#define GASGAUGE_REG_CTRL                 0x01 /* Control and Status Register   */
#define GASGAUGE_REG_CHARGE_LOW           0x02 /* Gas Gauge Charge Data Bits 0-7*/
#define GASGAUGE_REG_CHARGE_HIGH          0x03 /* Gas Gauge Charge Data Bits 8-15*/
#define GASGAUGE_REG_COUNTER_LOW          0x04 /* Number of Conversion Bits 0-7*/
#define GASGAUGE_REG_COUNTER_HIGH         0x05 /* Number of Conversion Bits 8-15*/
#define GASGAUGE_REG_CURRENT_LOW          0x06 /* Battery Current Value Bits 0-7*/
#define GASGAUGE_REG_CURRENT_HIGH         0x07 /* Battery Current Value Bits 8-15*/
#define GASGAUGE_REG_VOLTAGE_LOW          0x08 /* Battery Voltage Value Bits 0-7*/
#define GASGAUGE_REG_VOLTAGE_HIGH         0x09 /* Battery Voltage Value Bits 8-15*/
#define GASGAUGE_REG_TEMPERATURE_LOW      0x0A /* Temperature Values Bits 0-7) */
#define GASGAUGE_REG_TEMPERATURE_HIGH     0x0B /* Temperature Values Bits 8-15)*/

/* Device ID registers Address 24 to 31 --------------------------------------*/

#define GASGAUGE_REG_ID0                  0x18 /* Part Type ID 10h  */
#define GASGAUGE_REG_ID1                  0x19 /* Unique Part ID Bits 0-7  */
#define GASGAUGE_REG_ID2                  0x1A /* Unique Part ID Bits 8-15  */
#define GASGAUGE_REG_ID3                  0x1B /* Unique Part ID Bits 16-23  */
#define GASGAUGE_REG_ID4                  0x1C /* Unique Part ID Bits 24-31  */
#define GASGAUGE_REG_ID5                  0x1D /* Unique Part ID Bits 32-39  */
#define GASGAUGE_REG_ID6                  0x1E /* Unique Part ID Bits 40-47  */
#define GASGAUGE_REG_ID7                  0x1F /* Device ID CRC  */

/*    General Purpose RAM Register Address 32-63     */
#define GASGAUGE_RAM_SIZE                 32   /* Total RAM register of STC3100 */
#define GASGAUGE_REG_RAM0                 0x20
#define GASGAUGE_REG_RAM2                 0x22
#define GASGAUGE_REG_RAM4                 0x24
#define GASGAUGE_REG_RAM6                 0x26
#define GASGAUGE_REG_RAM8                 0x28
#define GASGAUGE_REG_RAM12                0x2C
#define GASGAUGE_REG_RAM14                0x2E
#define GASGAUGE_REG_RAM16                0x30
#define GASGAUGE_REG_RAM18                0x32
#define GASGAUGE_REG_RAM20                0x34
#define GASGAUGE_REG_RAM22                0x36
#define GASGAUGE_REG_RAM24                0x38
#define GASGAUGE_REG_RAM26                0x3A
#define GASGAUGE_REG_RAM28                0x3C
#define GASGAUGE_REG_RAM29                0x3D
#define GASGAUGE_REG_RAM30                0x3E
#define GASGAUGE_REG_RAM31                0x3F

/* GAS GAUGE variables definition ------------------------------------*/

typedef enum
{
  GASGAUGE_OK = 0u,
  GASGAUGE_ERROR,
  GASGAUGE_ERROR_BUFFER,
  GASGAUGE_COMMS_TIMEOUT,
  GASGAUGE_DATANOTREADY,
  GASGAUGE_BATT_CHARGING,
  GASGAUGE_BATT_NOTCHARGING
} GASGAUGE_status_t;

typedef struct
{
  int16_t BattVoltage;     // battery voltage in mV
  int16_t BattCurrent;     // battery current in mA
  int16_t BattTemperature; // battery temperature in 0.1°C
  int16_t BattChargeCount; // Coulomb counter value in mAh
  int16_t BattCounter;     // conv counter
  int16_t CoulombCount;    // Coulomb count in counts
} GASGAUGE_BatteryStatus_t;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
GASGAUGE_status_t GasGaugeInit                (void);
GASGAUGE_status_t GasGaugeLPSleepRestore      (void);
GASGAUGE_status_t GasGuageResetAccumulator    (void);
GASGAUGE_status_t GASGAUGE_ReadByte           (uint8_t startAddress, uint8_t *value);
GASGAUGE_status_t GASGAUGE_ReadWord           (uint8_t startAddress, uint16_t *value);
GASGAUGE_status_t GASGAUGE_WriteByte          (uint8_t startAddress, uint8_t value);
GASGAUGE_status_t GASGAUGE_WriteWord          (uint8_t startAddress, uint16_t value);
GASGAUGE_status_t GASGAUGE_Startup            (void);
GASGAUGE_status_t GASGAUGE_Powerdown          (void);
bool              GASGAUGE_WUST               (void);
GASGAUGE_status_t ReadBatteryDataFromGasGauge (GASGAUGE_BatteryStatus_t *batteryData);

/* Console commands */
int GasGaugeConsolePoke (int argCount, char **pArguments);
int GasGaugeConsolePeek (int argCount, char **pArguments);


#endif /* GASGAUGE_H */
