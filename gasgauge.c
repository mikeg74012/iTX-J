/**
 **************************************************************************************************
 * @file   gasgauge.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   25/03/2020
 *
 * @brief  Gas Gauge Driver (STC3100) to measure battery voltage and charge status
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */

#define  GASGUAGE_ID_PN_MASK       0x10     /* Vender identifier reported by GASGuage Chip */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static GASGAUGE_status_t ReadDataFromGasGauge (uint8_t startAddress, uint8_t datasize, uint8_t *dataItemPtr);
static GASGAUGE_status_t WriteDataToGasGauge  (uint8_t *bufferPtr, uint8_t dataSize);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Initialise the Gas Gauge
 * @param     i2cbus_e: I2cBus to use
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GasGaugeInit (void)
{
  GASGAUGE_status_t status = GASGAUGE_ERROR;

  status = GASGAUGE_Startup();

  if (status == GASGAUGE_OK)
  {
    TRACE_("GAS_GUAGE", "Gas Gauge Initialised Successfully\n\r");
    /* results available */
  }
  else
  {
    TRACE_("GAS_GUAGE", "Error in Gas Gauge Initialisation\n\r");
  }
  return status;
}


/**
 * @brief     Restore the Gas Gauge after LP reset
 * @param     i2cbus_e: I2cBus to use
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GasGaugeLPSleepRestore (void)
{
  GASGAUGE_status_t status = GASGAUGE_OK;
  uint8_t byteRead;
  int loopCount = 3;

  /* check the presence of the GAS GAUGE by reading first byte of the Device ID, repeat 3 times  */
  for(int i = 0; i < loopCount; i++)
  {
    if (GASGAUGE_ReadByte(GASGAUGE_REG_ID0, &byteRead) == GASGAUGE_OK)
    {
      if (byteRead == GASGUAGE_ID_PN_MASK)
      {
        break;  /* get the correct ID */
      }
      else
      {
        status = GASGAUGE_ERROR;
      }
    }
    else
    {
      status = GASGAUGE_DATANOTREADY;
    }
  }

  if (status != GASGAUGE_OK)
  {
    StatusLogWriteEventCode(STATUS_BATTERY_ERROR, status);  /* log battery gauge error */
  }
  return (status);
}

/**
 * @brief     Reset the charge accumulator and conversion counter
 * @param     none
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GasGuageResetAccumulator (void)
{
  return GASGAUGE_WriteByte(GASGAUGE_REG_CTRL, 0x02);
}


/**
 * @brief     Function to read a byte value stored in register
 * @param     startAddress: GAS GAUGE register address, value: pointer to 8-bit value to read
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_ReadByte (uint8_t startAddress, uint8_t *value)
{
  uint8_t dataPtr[2];

  GASGAUGE_status_t status = ReadDataFromGasGauge(startAddress, 1, dataPtr);

  if (status == GASGAUGE_OK)
  {
    *value = dataPtr[0];
  }
  else
  {
    // error reading value
  }

  return (status);
}


/**
 * @brief     Function to read a 16-bit value stored in a Gas Gauge register pair
 * @param     startAddress: GAS GAUGE register, value: pointer to 16-bit value to read
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_ReadWord (uint8_t startAddress, uint16_t *value)
{
  uint8_t dataPtr[2];

  GASGAUGE_status_t status = ReadDataFromGasGauge(startAddress, 2, dataPtr);

  if (status == GASGAUGE_OK)
  {
    *value = dataPtr[0];
    *value |= ((int16_t) dataPtr[1]) << 8;
  }
  else
  {
    //error
  }

  return (status);
}


/**
 * @brief     Function to write a byte value into a register
 * @param     startAddress: GAS GAUGE register, value: 8-bit value to write
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_WriteByte (uint8_t startAddress, uint8_t value)
{

  uint8_t buffer[] = { startAddress, value };

  return (WriteDataToGasGauge(buffer, 2));
}


/**
 * @brief     Function to write a 16-bit value into Gas Gauge register pair
 * @param     startAddress: GAS GAUGE register, value: 16-bit value to write
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_WriteWord (uint8_t startAddress, uint16_t value)
{
  uint8_t buffer[] = { startAddress, value & 0xFF, value >> 8 };

  GASGAUGE_status_t status = WriteDataToGasGauge(buffer, 3);

  return (status);
}


/**
 * @brief     Function to initialise the Gas Gauge at Power up (application startup)
 * @param     None
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_Startup (void)
{
  GASGAUGE_status_t status = GASGAUGE_ERROR;
  uint8_t byteRead;

  // first, check the presence of the GAS GAUGE by reading first byte of the Device ID
  if (GASGAUGE_ReadByte(GASGAUGE_REG_ID0, &byteRead) == GASGAUGE_OK)
  {
    if (byteRead == GASGUAGE_ID_PN_MASK)
    {
      // read the REG_CTRL to reset the GG_EOC and VTM_EOC bits
      GASGAUGE_ReadByte(GASGAUGE_REG_CTRL, &byteRead);

      // write 0x02 into the REG_CTRL to reset the accumulator and counter and clear the PORDET bit,
      if (GASGAUGE_WriteByte(GASGAUGE_REG_CTRL, 0x02) == GASGAUGE_OK)
      {
        // then write 0x10 into the REG_MODE register to start the GAS GAUGE in 14-bit resolution mode.
        if (GASGAUGE_WriteByte(GASGAUGE_REG_MODE, 0x10) == GASGAUGE_OK)
        {
          status = GASGAUGE_OK;
        }
      }
    }
  }
  return (status);
}


/**
 * @brief     Function to shut down the Gas Gauge at Power down
 *            This is will put the device in standby mode i.e. Accumulator and counter
 *            registers are frozen, gas gauge and battery monitor functions are in standby
 * @param     None
 * @return    GASGAUGE_OK on success
 */
GASGAUGE_status_t GASGAUGE_Powerdown (void)
{
  // write 0 into the REG_MODE register to put the GAS GAUGE in standby mode
  return (GASGAUGE_WriteByte(GASGAUGE_REG_MODE, 0));
}


/**
 * @brief     Function to read the battery data from GAS GAUGE device
 * @param     GASGAUGE_BatteryStatus_t batteryData: pointer to battery data status
 * @return    GASGAUGE_OK on success,
 * @affects   global battery variables
 */
GASGAUGE_status_t ReadBatteryDataFromGasGauge (GASGAUGE_BatteryStatus_t *batteryData)
{
  uint8_t dataPtr[12];
  int16_t value;
  GASGAUGE_BatteryStatus_t batteryDataValue = { 0 };

  // read GAS GAUGE registers 0 to 11
  GASGAUGE_status_t status = ReadDataFromGasGauge(0, 12, dataPtr);
  if (status == GASGAUGE_OK)
  {
    // fill the battery status data
    // charge count
    value = dataPtr[3];
    value = (value << 8) + dataPtr[2];
    batteryDataValue.CoulombCount = value;
    batteryDataValue.BattChargeCount = (int16_t)(((float)value * 6.7f) / GASGAUGE_SENSERESISTOR);  // value in mAh

    // conversion counter
    value = dataPtr[5];
    value = (value << 8) + dataPtr[4];
    batteryDataValue.BattCounter = value;

    // current
    value = dataPtr[7];
    value = (value << 8) + dataPtr[6];
    value &= 0x3fff; // mask unused bits
    if (value >= 0x2000)
    {
      value -= 0x4000;  // convert to signed value
    }
    batteryDataValue.BattCurrent = (int16_t)(((float)value * 11.77f) / GASGAUGE_SENSERESISTOR); // value in mA

    // voltage
    value = dataPtr[9];
    value = (value << 8) + dataPtr[8];
    value &= 0x0fff; // mask unused bits
    if (value >= 0x0800)
    {
      value -= 0x1000;  // convert to signed value
    }
    batteryDataValue.BattVoltage = (int16_t)((float)value * 2.44f); // value in mV

    // temperature
    value = (int16_t)dataPtr[11];
    value = (value << 8) + (int16_t)dataPtr[10];
    value &= 0x0fff; // mask unused bits
    if (value >= 0x0800)
    {
      value -= 0x1000;  // convert to signed value
    }
    batteryDataValue.BattTemperature = (int16_t)((float)value * 1.25f); // value in 0.1°C
  }
  else
  {
    // read failed from device
  }

  *batteryData = batteryDataValue;
  return (status);
}


/**
 * @brief     Gas Gauge Wake Up Self Test Routine
 * @param     none
 * @return    true - on success
 *            false - on failure
 */
bool GASGAUGE_WUST (void)
{
  bool result = false;
  uint8_t byteRead;

  /* Check device is charging and has enough charge */
  if ((ChargerIsChargingEnabled() == BATTERYCHARGER_ENABLED) && (BatteryMonitorGetLevel() > 0))
  {
    // Check the presence of the GAS GAUGE by reading first byte of the Device ID
    if (GASGAUGE_ReadByte(GASGAUGE_REG_ID0, &byteRead) == GASGAUGE_OK)
    {
      if (byteRead == 0x10)
      {
        result = true;
      }
    }
  }
  return result;
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */
/**
 * @brief     Reads data from the Gas Gauge
 * @param     startAddress: GAS GAUGE register address
 * @param     dataSize: Size of data to read
 * @param     datatItemPtr: pointer to data returned
 * @return    GASGAUGE_status_t - GASGAUGE_OK on success
 */
static GASGAUGE_status_t ReadDataFromGasGauge (uint8_t startAddress, uint8_t dataSize, uint8_t *dataItemPtr)
{
  GASGAUGE_status_t status = GASGAUGE_OK;

  if (dataItemPtr == NULL)
  {
    status = GASGAUGE_ERROR_BUFFER;
  }
  else
  {
#ifdef GD32F30X_HD   /* may blocking, no timeout option ! */
    if(!I2c_read_GG_bytes  (GasGaugeBus, GASGAUGE_ADDRESS_8BIT, dataItemPtr, startAddress, dataSize))
    {
      status = GASGAUGE_ERROR;
    }
#else
    /* send the read address */
    if (!I2cTransmit(GasGaugeBus, (uint8_t) GASGAUGE_ADDRESS_8BIT, &startAddress, 1))
    {
      status = GASGAUGE_ERROR;
    }

    /* Read value from device */
    if (status == GASGAUGE_OK)
    {
      if (!I2cReceive(GasGaugeBus, (uint8_t) GASGAUGE_ADDRESS_8BIT, dataItemPtr, (uint16_t) dataSize))
      {
        status = GASGAUGE_ERROR;
      }
    }
#endif
  }

  if (status != GASGAUGE_OK)
  {
    StatusLogWriteEventCode(STATUS_BATTERY_ERROR, status);  /* log battery gauge error */
  }
  return status;
}


/**
 * @brief     Write data to the Gas Gauge
 * @param     bufferPtr: Pointer to data to written
 * @param     dataSize: size of data to be written
 * @return    GASGAUGE_status_t - GASGAUGE_OK on success
 */
static GASGAUGE_status_t WriteDataToGasGauge (uint8_t *bufferPtr, uint8_t dataSize)
{
  GASGAUGE_status_t status = GASGAUGE_OK;

  if (bufferPtr == (void*) 0)
  {
    status = GASGAUGE_ERROR_BUFFER;
  }
  else
  {
    /* Write the data to the device */
    if (!I2cTransmit(GasGaugeBus, (uint8_t) GASGAUGE_ADDRESS_8BIT, bufferPtr, dataSize))
    {
      status = GASGAUGE_ERROR;
    }
  }

  if (status != GASGAUGE_OK)
  {
    StatusLogWriteEventCode(STATUS_BATTERY_ERROR, status);  /* log battery gauge error */
  }

  return status;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Read and display the value of a STC3100 register
 *
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GasGaugeConsolePeek (int argCount, char **pArguments)
{
  int registerAddress;
  uint8_t dataPtr[2];

  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    /* get the address */
    sscanf(pArguments[0],  "%d", &registerAddress);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 63))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Read the register */
    GASGAUGE_status_t status = ReadDataFromGasGauge((uint8_t) registerAddress, 1, (uint8_t *) dataPtr);

    if (status == GASGAUGE_OK)
    {
      ConsolePrintf("\nAddr 0x%02x = 0x%02x\n\r", registerAddress, (int)dataPtr[0]);
    }
    else
    {
      ConsolePrintf("Error reading data from gas gauge\n\r");
    }

    return(CONSOLE_ERROR_OK);
   }

  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Write the given data value into a STC3100 register
 *
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GasGaugeConsolePoke (int argCount, char **pArguments)
{
  int registerAddress;
  int data;

  /* check for correct number of arguments */
  if (2 == argCount)
  {
    /* get the address and data */
    sscanf(pArguments[0],  "%d", &registerAddress);
    sscanf(pArguments[1],  "%d", &data);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 63))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Write the register */
    uint8_t buffer[] = { (uint8_t) registerAddress, (uint8_t) data };
    GASGAUGE_status_t status = WriteDataToGasGauge(buffer, 2);

    if (status != GASGAUGE_OK)
    {
      ConsolePrintf("Error writing data to gas gauge\n\r");
    }

    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}



