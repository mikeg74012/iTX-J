/**
 **************************************************************************************************
 * @file   batterycharger.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Battery Charger Driver (BQ24295) used to control charge to Battery
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
/* Debug trace for the battery charger */

#define TRACE_CHGR(...)

/*
#if TRACE_CHGR_ENABLE
  #define TRACE_CHGR(...) TRACE_COLOUR_(FG_GRAY, BG_DEFAULT, "CHARGER", __VA_ARGS__)
#else
  #define TRACE_CHGR(...)
#endif
*/

#define CHARGER_CHIP_INFO_ERROR     0x00

/* Charge current limit values */
#define BATTERYCHARGER_REG00_100mA  0x58
#define BATTERYCHARGER_REG00_1500mA 0x5D
#define BATTERYCHARGER_REG00_2000mA 0x5E

#define BATTERYCHARGER_REG08_VBUS_MASK          0xC0
#define BATTERYCHARGER_REG08_VBUS_USB_HOST      0x40
#define BATTERYCHARGER_REG08_VBUS_ADAPTER_HOST  0x80

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static i2cbus_e ChargerBus;

static uint8_t  ChargerFlag = 0;

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static ChargerStatus_t ChargerReadData        (uint8_t startAddress, uint8_t datasize, uint8_t *dataItemPtr);
static ChargerStatus_t ChargerWriteData       (uint8_t *bufferPtr, uint8_t dataSize);
static ChargerStatus_t ChargerReadByte        (uint8_t startAddress, uint8_t *value);
static ChargerStatus_t ChargerWriteByte       (uint8_t startAddress, uint8_t value);
static ChargerStatus_t ChargerTurnOffBATFET   (void);
static ChargerStatus_t ChargerDisableWatchdog (void);
static ChargerStatus_t ChargerEnableOTG       (void)  __attribute__((unused));
static ChargerStatus_t ChargerDisableOTG      (void);
static uint8_t         ChargerGetChipInfo     (void);
static ChargerStatus_t ChargerIOConfig        (void);
static ChargerStatus_t ChargerProfileSetup    (void);
static bool            ChargerIsGoodPower     (void);
static void            ChargerDPairEnabled    (void);
static void            ChargerDPairDisabled   (void);
static bool            ChargerIsHostDCP       (void);
static ChargerStatus_t ChargerUpdateCurrentLimitByMCUDetection (void);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Initialise the Battery Charger
 * @param     I2cBus : the i2c bus to use
 * @return    Chargerstatus_t on success
 */
ChargerStatus_t ChargerInit (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;

  /* Carry out Wake-Up Self-Test if required */
  PostWakeUpSelfTest(BATTERYCHARGER);
  
  /* Configure the GPIO pins used by the charger module */
  status = ChargerIOConfig();

  /*
   * Reset all charger registers to their default settings
   */
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerWriteByte(BATTERYCHARGER_REG01, 0x80);
  }

  /*
   * Turn off the charger's internal watchdog, otherwise it will helpfully reset all the registers
   * to default values every 40 seconds
   */
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerDisableWatchdog();
  }

  /*
   * Disable charge at first call to enable accurate first reading from gas gauge
   * (Evident from charger plugged in after hibernation)
   * Enabled after first call to battery read in battery monitor task
   */
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerDisableCharging();
  }
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerDisableOTG();
  }

  if (status == BATTERYCHARGER_OK)
  {
    /* setup the Charger Profile accordingly */
    status = ChargerProfileSetup();
  }

  if (status == BATTERYCHARGER_OK)
  {
    TRACE_CHGR("Charger Initialised Successfully\n\r");
    /* results available */
  }
  else
  {
    TRACE_CHGR("Error in Charger Initialisation\n\r");
  }
  return status;
}


/**
 * @brief     Restore the Battery Charger after LP sleep reset
 * @param     I2cBus : the i2c bus to use
 * @return    Chargerstatus_t on success
 */
ChargerStatus_t ChargerLPSleepRestore (void)
{
  ChargerStatus_t status = BATTERYCHARGER_OK;

  /* Configure the GPIO pins used by the charger module */
  status = ChargerIOConfig();

  return (status);
}


/**
 * @brief     Enable charging of the battery
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
ChargerStatus_t ChargerEnableCharging (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the power-on configuration register */
  status = ChargerReadByte(BATTERYCHARGER_REG01, &value);

  if (status == BATTERYCHARGER_OK)
  {
    /* Set Charge enable bit 4 of the power-on configuration register */
    value |= (1 << 4);
    status = ChargerWriteByte(BATTERYCHARGER_REG01, value);
  }
  TRACE_CHGR("Battery charging enabled\n\r");

  StatusLogWriteEventCode(STATUS_CHARGING_ENABLED, STATUS_DATA_LOAD_NONE);

  return (status);
}


/**
 * @brief     Disable charging of the battery
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
ChargerStatus_t ChargerDisableCharging (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the power-on configuration register */
  status = ChargerReadByte(BATTERYCHARGER_REG01, &value);

  if (status == BATTERYCHARGER_OK)
  {
    /* Clear Charge enable bit 4 of the power-on configuration register */
    value &= ~(1 << 4);
    status = ChargerWriteByte(BATTERYCHARGER_REG01, value);
  }
  TRACE_CHGR("Battery charging disabled\n\r");

  StatusLogWriteEventCode(STATUS_CHARGING_DISABLED, STATUS_DATA_LOAD_NONE);

  return (status);
}


/**
 * @brief     Turn off Charger BATFET so that the system voltage is zero to minimise the leakage during "hibernate" mode
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
ChargerStatus_t ChargerEnterShippingMode (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;

  /*
   * Disable the watchdog timer in Charge Termination/Timer Control Register
   * required to keep BATFET off during shipping mode
   */
  status = ChargerDisableWatchdog();

  /* Turn off BATFET to disconnect the battery and enter shipping mode */
  if (BATTERYCHARGER_OK == status)
  {
    status = ChargerTurnOffBATFET();
  }

  return (status);
}


/**
 * @brief     Gets whether charging is enabled or disabled from Battery Charger
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_ENABLED or BATTERYCHARGER_DISABLED on success
 */
ChargerStatus_t ChargerIsChargingEnabled (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the power-on configuration register */
  status = ChargerReadByte(BATTERYCHARGER_REG01, &value);

  if (status == BATTERYCHARGER_OK)
  {
    /* Check Charge enable bit 4 of the power-on configuration register */
    if ((value & (1 << 4)) != 0)
    {
      status = BATTERYCHARGER_ENABLED;
    }
    else
    {
      status = BATTERYCHARGER_DISABLED;
    }
  }

  return (status);
}

/**
 * @brief     Charger IC tell it is fully charged.
 * @param     none
 * @return    true - on success
 *            false - on failure
 */
bool ChargerIsFullyCharged (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the system status register */
  status = ChargerReadByte(BATTERYCHARGER_REG08, &value);

  if (status == BATTERYCHARGER_OK)
  {
    return ((value &= REG08_CHRG_STAT_MASK) == REG08_CHRG_STAT_FULLY_CHARGE);
  }
    else
  {
    return false;
  }
}


/**
 * @brief     Query the charger IC if charging is in progress
 * @param     none
 * @return    true - if any charging process is in place, including fully charge state
 *            false - if no charging, or charge IC communication error
 */
bool ChargerIsChargingInProgress (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the system status register */
  status = ChargerReadByte(BATTERYCHARGER_REG08, &value);

  if (status == BATTERYCHARGER_OK)
  {
    return ((value &= REG08_CHRG_STAT_MASK) != REG08_CHRG_STAT_NOT_CHARGING);
  }
    else
  {
    return false;
  }
}


/**
 * @brief     Battery Charger Wake Up Self Test Routine
 * @param     none
 * @return    true - on success
 *            false - on failure
 */
bool ChargerWUST (void)
{
  uint8_t chipInfo = ChargerGetChipInfo();

  /* Self test success if chip info can get */
  if (chipInfo != CHARGER_CHIP_INFO_ERROR)
  {
    TRACE_CHGR("Charge IC Chip Info 0x%02x\n\r", chipInfo);
    return true;
  }
  else
  {
    return false;
  }
}


/**
 * @brief     Detect and set the Input Current Limit
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
ChargerStatus_t ChargerSetInputCurrentLimit (void)
{
    uint8_t value = 0xff;
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t chipInfo = ChargerGetChipInfo();

  if (!BatteryMonitorGetUsbState())
  {
    // Default the input current limit to 100mA when unplug or first boot
    status = ChargerWriteByte(BATTERYCHARGER_REG00, BATTERYCHARGER_REG00_100mA);

    if (chipInfo == BATTERCHARGER_BQ24292_PN_MASK)
    {
      /* set input current limit to 100mA */
        
        set_gpio_high (CHRGR_PSEL);
    }
    /* Reset the Port Status when unplugged */
    ChargerFlag &= (~BATTERY_CHARGER_DCP_PORT);
  }
  else
  {
    /* Usb Plugged */
    switch (chipInfo)
    {
      case BATTERCHARGER_BQ24292_PN_MASK:
        ChargerFlag |= BATTERY_CHARGER_BQ24292;
        /* Set the current limit by the MCU Detection */
        status = ChargerUpdateCurrentLimitByMCUDetection();

      break;
      case BATTERCHARGER_BQ24295_PN_MASK:
        /* still set to input current limit by I2C */
        ChargerFlag |= BATTERY_CHARGER_BQ24295;
        TRACE_CHGR("DCP detection done by HW\n\r");

        /* Default the charging current, Charger IC will update reg00 after hardware detection */
        status = ChargerWriteByte(BATTERYCHARGER_REG00, BATTERYCHARGER_REG00_2000mA);

        /* poll for the Charger IC for the finish of the detection
         * It usually take < 300ms */
        for (uint8_t i =0; i < 50; i++)
        {
            status = ChargerReadByte(BATTERYCHARGER_REG08, &value);
            if (status != BATTERYCHARGER_OK)
            {
              /* Charger IC not response, give the last shot to set the current limit to low */
              status = ChargerWriteByte(BATTERYCHARGER_REG00, BATTERYCHARGER_REG00_100mA);
              return status;
            }
            else if ((value & BATTERYCHARGER_REG08_VBUS_MASK) != 0)
            {
              /* Detection result ready to use */
              if ((value & BATTERYCHARGER_REG08_VBUS_MASK) == BATTERYCHARGER_REG08_VBUS_ADAPTER_HOST )
              {
                ChargerFlag |= BATTERY_CHARGER_DCP_PORT;
              }
              else
              {
                ChargerFlag &= (~BATTERY_CHARGER_DCP_PORT);
              }
              break;
            }
            sleep(20);
        }

        /* Cover for the its088 BQ24295 Board misplacement of Resistor network on HW F*/
        if (isPCBVersionF())
        {
          /* Check what the HW Detection return */
          if ((value & BATTERYCHARGER_REG08_VBUS_MASK) == BATTERYCHARGER_REG08_VBUS_USB_HOST )
          {
            TRACE_CHGR("HW detection with USB Port, Retest with Software\n\r");
            status = ChargerUpdateCurrentLimitByMCUDetection();
          }
        }
        break;
      default:
        break;
    }
  }
  return status;
}


/**
 * @brief     return detected charge status flag
 * @param     none
 * @return    uint8_t: current charge status flag
 */
uint8_t ChargeStatusFlag (void)
{
  return (ChargerFlag);
}


/**
 * @brief     Check the charge chip is that of BQ24292
 *            note: I2C bus must setup and ready before call this function
 * @param     none
 * @return    true if confirmed
 */
bool ChargerIsBQ24292 (void)
{
  return (BATTERCHARGER_BQ24292_PN_MASK == ChargerGetChipInfo());
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */
/**
 * @brief     Function to read a byte value stored in register
 * @param     startAddress: Battery Charger register address, value: pointer to 8-bit value to read
 * @return    BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerReadByte (uint8_t startAddress, uint8_t *value)
{
  uint8_t dataPtr[2];

  ChargerStatus_t status = ChargerReadData(startAddress, 1, dataPtr);

  if (status == BATTERYCHARGER_OK)
  {
    *value = dataPtr[0];
  }
  else
  {
    /* error reading value */
  }

  return (status);
}


/**
 * @brief     Function to write a byte value into a register
 * @param     startAddress: Battery Charger register, value: 8-bit value to write
 * @return    BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerWriteByte (uint8_t startAddress, uint8_t value)
{
  uint8_t buffer[] = { startAddress, value };

  return (ChargerWriteData(buffer, 2));
}


/**
 * @brief     Initialise Charger related IO, Chips Specific
 * @param     None
 * @return    None
 */
static ChargerStatus_t ChargerIOConfig (void)
{
  if (ChargerGetChipInfo() == BATTERCHARGER_BQ24292_PN_MASK)
  {
    /*Configure GPIO pin for Charge power selection output */
    
    set_gpio_high (CHRGR_PSEL);
    set_gpio_output (CHRGR_PSEL);
  }

  return BATTERYCHARGER_OK;
}


/**
 * @brief     Initialise D+/D- for current detection
 * @param     None
 * @@return   None
 */
static void ChargerDPairEnabled (void)
{
  set_gpio_output (USB_D_P);
  
  /*Configure GPIO pin : D- for Input */
  
  set_gpio_input (USB_D_N);
  
}


/**
 * @brief     DeInitialise D+/D- for current detection
 * @param     None
 * @return    None
 */
static void ChargerDPairDisabled (void)
{
  /* Reset D+ pin */
  set_gpio_input (USB_D_P);
  
  /* Reset D- pin */
  set_gpio_input (USB_D_N);
  
}


/**
 * @brief     Reads data from the Battery Charger
 * @param     StartAddress : Battery Charger register address
 * @param     DataSize     : Size of data to read
 * @param     pDataItem : pointer to data returned
 * @return    ChargerStatus_t : BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerReadData (uint8_t StartAddress, uint8_t DataSize, uint8_t *pDataItem)
{
  ChargerStatus_t status = BATTERYCHARGER_OK;

  if (pDataItem == NULL)
  {
    status = BATTERYCHARGER_ERROR_BUFFER;
  }
  else
  {
    /* send the read address */
    if (!I2cTransmit(ChargerBus, (uint8_t) BATTERYCHARGER_ADDRESS, &StartAddress, 1))
    {
      status = BATTERYCHARGER_ERROR;
    }

    // Read value from device
    if (status == BATTERYCHARGER_OK)
    {
      if (!I2cReceive(ChargerBus, (uint8_t) BATTERYCHARGER_ADDRESS, pDataItem, (uint16_t) DataSize))
      {
        status = BATTERYCHARGER_ERROR;
      }
    }
  }
  return status;
}


/**
 * @brief     Write data to the Battery Charger
 * @param     pBuffer  : Pointer to data to written
 * @param     DataSize : size of data to be written
 * @return    ChargerStatus_t : BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerWriteData (uint8_t * pBuffer, uint8_t DataSize)
{
  ChargerStatus_t status = BATTERYCHARGER_OK;

  if (pBuffer == NULL)
  {
    status = BATTERYCHARGER_ERROR_BUFFER;
  }
  else
  {
    /* Write the data to the device */
    if (!I2cTransmit(ChargerBus, (uint8_t) BATTERYCHARGER_ADDRESS, pBuffer, DataSize))
    {
      status = BATTERYCHARGER_ERROR;
    }
  }
  return status;
}


/**
 * @brief     Turn off the Charger's internal watchdog.  Note, the watchdog will
 *            periodically reset all of the charger registers to their default values
 *            unless it is disabled, or kicked every few seconds.
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerDisableWatchdog (void)
{
  uint8_t value;

  /* get the current register value */
  ChargerStatus_t status = ChargerReadByte((uint8_t)BATTERYCHARGER_REG05, &value);

  /* clear bits 4 and 5 to disable the internal watchdog */
  if (BATTERYCHARGER_OK == status)
  {
    /* set (REG05[5:4] = 00) */
    value &= ~(1 << 4);
    value &= ~(1 << 5);
    status = ChargerWriteByte((uint8_t)BATTERYCHARGER_REG05, value);
  }
  return (status);
}


/**
 * @brief     Turn off Charger BATFET so that the system voltage is zero
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerTurnOffBATFET (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the Misc Operation Control Register */
  if (ChargerReadByte(BATTERYCHARGER_REG07, &value) == BATTERYCHARGER_OK)
  {
    /* Set BATFET_DISABLE bit of the Misc Operation Control Register */
    value |= (1 << 5);
    status = ChargerWriteByte(BATTERYCHARGER_REG07, value);
  }

  return (status);
}


/**
 * @brief     Enable OTG power output
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerEnableOTG (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the power-on configuration register */
  status = ChargerReadByte(BATTERYCHARGER_REG01, &value);

  if (status == BATTERYCHARGER_OK)
  {
    /* Set OTG enable bit 5 of the power-on configuration register */
    value |= (1 << 5);
    status = ChargerWriteByte(BATTERYCHARGER_REG01, value);
  }
  TRACE_CHGR("OTG enabled\n\r");

  return (status);
}


/**
 * @brief     Disable OTG power output
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerDisableOTG (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  uint8_t value;

  /* Read the power-on configuration register */
  status = ChargerReadByte(BATTERYCHARGER_REG01, &value);

  if (status == BATTERYCHARGER_OK)
  {
    /* Clear OTG enable bit 5 of the power-on configuration register */
    value &= ~(1 << 5);
    status = ChargerWriteByte(BATTERYCHARGER_REG01, value);
  }
  TRACE_CHGR("OTG disabled\n\r");

  return (status);
}


/**
 * @brief     Apply the charger Profile according to the Host type attached
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerProfileSetup (void)
{
  ChargerStatus_t status = BATTERYCHARGER_ERROR;

  /*  Detect and set the Input Current Limit */
  status = ChargerSetInputCurrentLimit();

  /* set charge current limit to 1.25A (0.5C) */
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerWriteByte(BATTERYCHARGER_REG02, 0x30);
  }

  /* set Charge Voltage Limit to 4.208V (Default value of charger IC may be different */
  if (status == BATTERYCHARGER_OK)
  {
    status = ChargerWriteByte(BATTERYCHARGER_REG04, 0xB2);
  }

  TRACE_CHGR("Charger Profile Applied with status %d\n\r", status);

  return (status);
}


/**
 * @brief     Return the Chip Info of Charger IC
 * @param     none
 * @return    CHARGER_CHIP_INFO_ERROR on failed, Chip ID on success
 */
static uint8_t ChargerGetChipInfo (void)
{
  uint8_t response = CHARGER_CHIP_INFO_ERROR;
  ChargerStatus_t status = ChargerReadByte(BATTERYCHARGER_REG0A, &response);

  /* Read the device revision status register for response */
  if (status == BATTERYCHARGER_OK)
  {
    if ((response & BATTERCHARGER_BQ24295_PN_MASK) == BATTERCHARGER_BQ24295_PN_MASK)
    {
      response = BATTERCHARGER_BQ24295_PN_MASK;
    }
    else if ((response & BATTERCHARGER_BQ24292_PN_MASK) == BATTERCHARGER_BQ24292_PN_MASK)
    {
      response = BATTERCHARGER_BQ24292_PN_MASK;
    }
    else
    {
      TRACE_CHGR("Charger IC Unknown 0x%02x\n\r",response);
      response = CHARGER_CHIP_INFO_ERROR;
    }
  }
  else
  {
    TRACE_CHGR("Charger IC Read Error\n\r");
    response = CHARGER_CHIP_INFO_ERROR;
  }

  return response;
}


/**
 * @brief     Check if Good Power Delivered to the VBus
 * @param     none
 * @return    true if power is good, false otherwise
 */
static bool ChargerIsGoodPower (void)
{
  uint8_t response = 0x00;
  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  bool isGoodPower = false;

  for (uint8_t retry = 0; retry < 100; retry++)
  {
    /* Read the device revision status register for response */
    status = ChargerReadByte(BATTERYCHARGER_REG08, &response);
    if ( status == BATTERYCHARGER_OK)
    {
      isGoodPower = (response & REG08_PG_STAT_MASK) >> REG08_PG_STAT_MASK_POS;
    }

    if ((isGoodPower) || (status != BATTERYCHARGER_OK))
    {
      /* Exit loop immediately if power is good, or chip did not respond */
      break;
    }

    /* Give a wait to see if a good power can be delivered */
    sleep(5);
  }

  return isGoodPower;
}


/**
 * @brief     Check if Connected Host is DCP or not
 * @param     none
 * @return    true if DCP is connected, false otherwise
 */
static bool ChargerIsHostDCP (void)
{
  bool ret = false;

  /* No Further action if power is not a good one */
  if (!ChargerIsGoodPower())
  {
    TRACE_CHGR("Non-DCP Host Connected\n\r");
    return ret;
  }

  /* Enable the Data Pair IO for Current Detection */
  ChargerDPairEnabled();
  
  /* Output high to D+ for 40ms */
  set_gpio_high (USB_D_P);
  sleep(40);  /* Timing from BCD 1.2 */

  /* If D+/D- are high, it is DCP, otherwise, it is either SDP or CDP */
  if (!(read_gpio (USB_D_N)))
  {
    TRACE_CHGR("Non-DCP Host Connected\n\r");
    ret = false;
  }
  else
  {
    TRACE_CHGR("DCP Host Connected\n\r");
    ret = true;
  }

  /* Disable the Data Pair IO after use */
  ChargerDPairDisabled();

  return ret;
}


/**
 * @brief     Update the Charge Current Limit by the MCU Detection
 *            Used for the IC BQ24292I, also
 *            Cover for the its088 BQ24295 Board misplacement of Resistor network
 * @param     none
 * @return    Chargerstatus_t - BATTERYCHARGER_OK on success
 */
static ChargerStatus_t ChargerUpdateCurrentLimitByMCUDetection (void)
{

  ChargerStatus_t status = BATTERYCHARGER_ERROR;
  if (ChargerIsHostDCP())
  {
    /* set to input current limit 1.5A */
      
      set_gpio_low (CHRGR_PSEL);
    TRACE_CHGR("set input current limit to 1.5A\n\r");
    status = ChargerWriteByte(BATTERYCHARGER_REG00, BATTERYCHARGER_REG00_1500mA);
    ChargerFlag |= BATTERY_CHARGER_DCP_PORT;
  }
  else
  {
    /* set input current limit to 100mA */
      set_gpio_high (CHRGR_PSEL);
    TRACE_CHGR("set input current limit to 100mA\n\r");
    status = ChargerWriteByte(BATTERYCHARGER_REG00, BATTERYCHARGER_REG00_100mA);
    ChargerFlag &= (~BATTERY_CHARGER_DCP_PORT);
  }

  return status;
}

/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Display current charger status
 *
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ChargerConsoleStatus (int argCount, char **pArguments)
{
  ChargerStatus_t status = ChargerIsChargingEnabled();

  ConsolePrintf("Charger IC Info: 0x%02x\n\r", ChargerGetChipInfo());

  switch (status)
  {
  case BATTERYCHARGER_ENABLED:
    ConsolePrintf("Charger is Enabled\n\r");
    break;
  case BATTERYCHARGER_DISABLED:
    ConsolePrintf("Charger is Disabled\n\r");
    break;
  default:
    ConsolePrintf("Charger error\n\r");
  }

  ConsolePrintf("Charger IC is charging progress? %s\n\r", ChargerIsChargingInProgress()?"yes":"no");

  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Enable the charger
 *
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ChargerConsoleEnable (int argCount, char **pArguments)
{
  if (ChargerEnableCharging() == BATTERYCHARGER_OK)
  {
    ConsolePrintf("Charger Enabled\n\r");
  }
  else
  {
    ConsolePrintf("Error in enabling Charger\n\r");
  }
  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Disable the charger
 *
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ChargerConsoleDisable (int argCount, char **pArguments)
{
  if (ChargerDisableCharging() == BATTERYCHARGER_OK)
  {
    ConsolePrintf("Charger Disabled\n\r");
  }
  else
  {
    ConsolePrintf("Error in enabling Charger\n\r");
  }
  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Read and display the value of a BQ24295 register
 *
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ChargerConsolePeek (int argCount, char **pArguments)
{
  int registerAddress;
  uint8_t data = 0;

  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    /* get the address */
    sscanf(pArguments[0],  "%x", &registerAddress);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 0x0A))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Read the register */
    if (BATTERYCHARGER_OK == ChargerReadByte((uint8_t)registerAddress, &data))
    {
      ConsolePrintf("\nAddr 0x%02x = 0x%02x\n\r", registerAddress, (int)data);
    }
    else
    {
      ConsolePrintf("Error reading data\n\r");
    }

    return(CONSOLE_ERROR_OK);
   }

  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Write the given data value into a BQ24295 register
 *
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ChargerConsolePoke (int argCount, char **pArguments)
{
  int registerAddress;
  int data;

  /* check for correct number of arguments */
  if (2 == argCount)
  {
    /* get the address and data */
    sscanf(pArguments[0],  "%x", &registerAddress);
    sscanf(pArguments[1],  "%x", &data);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 0x0A))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Write the register */
    if (BATTERYCHARGER_OK == ChargerWriteByte((uint8_t)registerAddress, (uint8_t)data))
    {
      /* print the status data */
      ConsolePrintf("\nAddr 0x%02x = 0x%02x\n\r", registerAddress, data);
    }
    else
    {
      ConsolePrintf("Error writing data\n\r");
    }
    return (CONSOLE_ERROR_OK);
   }

   return (CONSOLE_ARG_NO);
}
