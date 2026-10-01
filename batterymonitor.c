/**
 **************************************************************************************************
 * @file   batterymonitor.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Battery Monitor Thread main file.
 *         Also handles watchdog functionality.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the Battery Monitor */
#define TRACE_BATMON(...)
/*
#if TRACE_BATMON_ENABLE
  #define TRACE_BATMON(...) TRACE_COLOUR_(FG_GREEN, BG_DEFAULT, "BAT", __VA_ARGS__)
#else
  #define TRACE_BATMON(...)
#endif
*/

/*
 * We check the charging state periodically
 * NOTES: 1. This time MUST match the RTC Wake-Up time for the LED flash to work properly
 *           We could tie them together by using the same macro.
 *        2. We could use different times for "sleep" and "wake" states
 */
#define CHARGING_CHECK_PERIOD_IN_MS      200        // 200 milliseconds

/* We check the battery level every 5 minutes */
#define FIRST_BATTERY_CHECK_IN_MS        4000        // need to be greater than 4s from start-up
#define BATTERY_CHECK_PERIOD_SECS        (1 * 60)    // 1 minutes

/* When we have a low battery we ask for a warning tone every 10 seconds */
#define LOW_BATTERY_TONE_INTERVAL_SECS   (10)        // 10 seconds

#define BATTERY_EMPTY_CAPACITY           0           // (in mAh)
#define BATTERY_FULL_CAPACITY            2500        // (in mAh)

/* when charging is complete, the charge current drops to a trickle below 50 mA */
#define CHARGE_COMPLETE_CURRENT_MA       50

#define CHARGE_PERCENT_MIN_ADDED         1

#define MINBATTCHARGETEMP                0      // Value in degree Celsius
#define MAXBATTCHARGETEMP                50     // Value in degree Celsius
#define BATCHARGETEMPOVERRANGE           10     // Value in degree Celsius

/* Battery voltages below which device operation must be modified */
#define BATTERY_LOW_THRESHOLD_MV         3450        // 3.45V - stop journeys
#define BATTERY_CRITICAL_THRESHOLD_MV    3350        // 3.35V - stop journeys and don't allow modem to power up (e.g. for heartbeat)
#define BATTERY_HIBERNATE_THRESHOLD_MV   3200        // 3.2V - hibernate the device if battery voltage below this limit
#define BATTERY_HYSTERESIS_MV            50          // hysteresis value used on thresholds
#define BATTERY_SLEEP_VOLTAGE_DIFF       100         /* threshold for filter low battery log after sleep */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* Data read from the battery */
static GASGAUGE_BatteryStatus_t batteryReadData = { 0 };

/* Current Charging Status */
static ChargingState_e ChargingState  = NOT_CHARGING;
static bool            ChargerEnabled = false;
static bool            UsbPresent     = false;
static bool            PrevUsbState   = false;

/* calculated battery stats */
static uint16_t batteryCapacitymAh  = 0;
static uint8_t  batteryLevelPercent = 100;
static ePowerState_t PowerState = POWER_STATE_NORMAL;

/* periodic battery logging to the console */
static bool BatteryLogging = true;

/* Indicate if Charging has been disabled due to temperature */
static bool ChargeTemperatureAllowed = false;
static bool ChargeConditionsChanged = false;
static bool ChargeStartLogDelayFlag = false;

/*
 * After USB power is connected or disconnected, we need to allow 4 seconds for charger to update
 * before  we take the next measurement (STC3100 datasheet, section 7.2)
 */
static int BatteryMeasurementHoldoff = 0;

/* battery charge record */
static uint32_t ChargeStartTimeStamp   = 0;
static uint8_t  ChargeStartPercent     = 0;
static uint32_t ChargeEndData          = 0;    /* time<<8 | percentage */

/*
 **************************************************************************************************
 *                                         PUBLIC VARIABLES
 **************************************************************************************************
 */
bool  UsbWakeUp;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void            BatteryMonitorConfigureGpio       (void);
static void            BatteryMonitorFirstCheck          (void);
static void            BatteryMonitorCheckBatteryState   (void);
static void            BatteryMonitorHandleCharging      (void);
static uint32_t        GetBatteryLevelIndicator          (int16_t BattCapacity);
static ChargingState_e BatteryMonitorGetChargingStatus   (void);
static void            BatteryMonitorPrintBatteryStatus  (void);
static void            BatteryMonitorPrintChargerStatus  (void);
static void            BatteryMonitorPrintPowerState     (void);
static void            BatteryMonitorEstimateCharge      (void);
static void            BatteryMonitorCheckUsbState       (void);
static void            BatteryMonitorSafetyChecks        (void);
static void            BatteryMonitorCalculatePowerState (void);
static void            SetChargeConditionsChanged        (bool newState);
static bool            UpdateUSBPresentState             (void);
static void            BatteryChargeStateUpdate          (void);
static void            UpdateLPSleepModeByChargeState    (void);
static uint32_t        BatteryChargeStatusPayloader      (void);


/*
 **************************************************************************************************
 *                                   GLOBAL / PUBLIC VARIABLES
 **************************************************************************************************
 */
bool watchdogResetDetected = false;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     The Battery Monitor Task function
 * @param     (not used)
 * @return    None
 */
void BatteryMonitorTask (void const *argument)
{
  uint32_t LastGasReadTime = 0;

  /* start the watchdog */
  WatchdogStart();

  BatteryMonitorConfigureGpio();
  /* Update current USB cable status */
  UpdateUSBPresentState();

  /* standby sleep mode check  */
  if (IsLowPowerSleepFlagUp())
  {
#ifdef GD32F30X_HD
      /* Disable wakeup pin */
      and_register (PMU, PMU_CS, ~(1 << 8));
#else
      HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);  /* lower power consumption */
#endif

    /* restore and update charge state */
    LastGasReadTime  = read_register (BKP, RTC_BACKUP_LAST_GASREADTIME);
    PrevUsbState     = (bool)(GetLPSleepStatus() & RTC_BKUP_USB_CABLE_FLAG);  /* read back previous state */
    ChargerEnabled   = (bool)(GetLPSleepStatus() & RTC_BKUP_CHARGER_FLAG);    /* read back previous charger enable state */
    /* restore the Battery Charger chip comms */
    ChargerLPSleepRestore();
    /*restore Gas gauge monitor to check battery level */
    GasGaugeLPSleepRestore();
    /* current battery state update */
    BatteryMonitorCheckBatteryState();
  }
  else
  {
    /* Initialise the Battery Charger  */
    if (BATTERYCHARGER_OK != ChargerInit())
    {
      StatusLogWriteEventCode(STATUS_CHARGING_ERROR, STATUS_DATA_LOAD_NONE);  /* log battery charge error */
    }

    /* Initialise the Gas gauge monitor to check battery level */
    if (GASGAUGE_OK != GasGaugeInit())
    {
      StatusLogWriteEventCode(STATUS_BATTERY_ERROR, STATUS_DATA_LOAD_NONE);  /* log battery gauge error */
    }

    /* After power up, we wait for 4 seconds, then perform one time checks and calibration */
    sleep(FIRST_BATTERY_CHECK_IN_MS);
    BatteryMonitorFirstCheck();
  }

  /* Infinite loop */
  for (;;)
  {
    WatchdogSetActive(BATTERYMONITORTASK);

    /* get the current time */
    uint32_t CurrentTimeStamp = RtcGetTimeStamp();
    if (BatteryMeasurementHoldoff) BatteryMeasurementHoldoff--;

    /* check the current state of USB power */
    BatteryMonitorCheckUsbState();
    /* Periodically, read the gas gauge */
    if ((CurrentTimeStamp-LastGasReadTime > BATTERY_CHECK_PERIOD_SECS) && (!BatteryMeasurementHoldoff))
    {
      /* Read the gas gauge */
      BatteryMonitorCheckBatteryState();

      /* Log status to console if required */
      if (BatteryLogging)
      {
        if (!IsLowPowerSleepFlagUp())
        {
          BatteryMonitorPrintBatteryStatus();
          BatteryMonitorPrintPowerState();
        }
      }

      /* force an read of charger status so we keep in sync */
      ChargeConditionsChanged = true;

      /* check the delayed log flag, it will have established charge current now */
      if (ChargeStartLogDelayFlag == true )
      {
        StatusLogWriteEventCode(STATUS_CHARGING_STARTED, BatteryChargeStatusPayloader());
        ChargeStartLogDelayFlag = false;
      }
      /* Update the last read time */
      LastGasReadTime = CurrentTimeStamp;
      write_register (BKP, RTC_BACKUP_LAST_GASREADTIME, LastGasReadTime);
    }

    /*
     * Update the charging status if charge conditions have changed.  Note, this happens when
     * the USB power or temperature has changed, or because we have just read the gas guage
     */
    if (ChargeConditionsChanged)
    {
      ChargeConditionsChanged = false;
      BatteryMonitorGetChargingStatus();

      /* Log status to console if required */
      if (BatteryLogging)
      {
        if (!IsLowPowerSleepFlagUp())
        {
          BatteryMonitorPrintChargerStatus();
        }
      }
    }

#ifdef LOW_BATTERY_TONES
    uint32_t LastLowBatTime = 0;

    /* Every 10 seconds, if the battery is low we send battery warning to sequencer */
    if (CurrentTimeStamp >= (LastLowBatTime + LOW_BATTERY_TONE_INTERVAL_SECS))
    {
      if ((NOT_CHARGING == ChargingState) && (batteryLevelPercent <= BATTERY_WARNING_THRESHOLD_PERCENT))
      {
        /* run the low battery sequencer */
        SequencerLowBattery();
      }
      /* Update the last read time */
      LastLowBatTime = CurrentTimeStamp;
    }
#endif

    /* Update the status of Battery on the LED */
    LedUpdateStatus();

    /* set the current power state (normal / low / critical) to control what device functions are allowed */
    BatteryMonitorCalculatePowerState();

    /* Manage charging of the battery */
    BatteryMonitorHandleCharging();

    /* Perform safety checks */
    BatteryMonitorSafetyChecks();

    /* Update watchdog and go to sleep */
    WatchdogSetWaiting(BATTERYMONITORTASK);
    sleep(CHARGING_CHECK_PERIOD_IN_MS);
  }
}


/**
 * @brief     Get the battery level
 * @param     None
 * @return    The battery level %
 */
uint8_t BatteryMonitorGetLevel (void)
{
  return batteryLevelPercent;
}


/**
 * @brief     Get the battery charging state
 * @param     None
 * @return    The battery charging state
 */
ChargingState_e BatteryMonitorGetChargingState (void)
{
  return ChargingState;
}


/**
 * @brief     Get the current USB power state
 * @param     None
 * @return    USB Power state : true = present, false = not present
 */
bool BatteryMonitorGetUsbState (void)
{
  return UsbPresent;
}


/**
 * @brief     Get the battery monitor temperature
 * @param     (none)
 * @return    The battery charging temperature in 0.1 degree steps
 */
int16_t BatteryMonitorGetTemperature (void)
{
  return batteryReadData.BattTemperature;
}

/**
 * @brief     Get the battery Voltage
 * @param     (none)
 * @return    The resolution is 2.44mV for the battery voltage
 */
int16_t BatteryMonitorGetVoltage (void)
{
  return batteryReadData.BattVoltage;
}


/**
 * @brief     Get the battery Current
 * @param     (none)
 * @return    The resolution is 11.77uA for the battery current
 */
int16_t BatteryMonitorGetCurrent (void)
{
  return batteryReadData.BattCurrent;
}


/**
 * @brief     Get the power state normal / low / critical
 * @param     (none)
 * @return    POWER_STATE_NORMAL   - all operations allowed
 *            POWER_STATE_LOW      - no journey allowed
 *            POWER_STATE_CRITICAL - no journey or modem power up allowed
 */
ePowerState_t BatteryMonitorGetPowerState (void)
{
  return PowerState;
}


/**
 * @brief     restore the power state from PSRAM
 * @param     pState: backup state
 * @return    none
 */
void BatteryRestorePowerState (int prevState)
{
  PowerState = (ePowerState_t) prevState;
}


/**
 * @brief     Get the charger enable state
 * @param     (none)
 * @return    bool of charger state
 */
bool  BatteryGetChargerState  (void)
{
  return ChargerEnabled;
}


/*
 * @brief   USB Power Present ISR
 * @param   None
 * @retval  None
 */
void BatteryMonitorUsbPowerISR (void)
{
  UpdateUSBPresentState();
  UsbWakeUp = true;
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     First read of battery status after MCU start up.  We don't have historical data
 *            so we must estimate the initial battery charge from its voltage
 * @param     None
 * @return    None
 */
static void BatteryMonitorFirstCheck (void)
{
  /* Check USB power state */
  UpdateUSBPresentState();

  /* Calculate estimated charge level based on known voltage discharge curve */
  BatteryMonitorEstimateCharge();

  /* Carry out Wake-Up Self-Test if required */
  PostWUSTSetBatteryInfo(batteryReadData);
  PostWakeUpSelfTest(GASGUAGE);
}


/**
 * @brief     Update the UsbPresent state by read the USB detect pin level.
 * @param     None
 * @return    None
 */
static bool UpdateUSBPresentState (void)
{
  /* Check USB power cable state */
  UsbPresent = read_gpio (USB_DETECT) ? true : false;
  return (UsbPresent);
}


/**
 * @brief     Read battery status from the gas gauge and calculate remaining capacity
 * @param     None
 * @return    None
 */
static void BatteryMonitorCheckBatteryState (void)
{
  uint16_t valueRef;
  int16_t BatterymAh = 0;  // Note, needs to be signed for calculations below

  /*  Read Saved initial reference value from Gas Gauge RAM */
  GASGAUGE_status_t status = GASGAUGE_ReadWord(GASGAUGE_REG_RAM0, &valueRef);

  if (status == GASGAUGE_OK)
  {
    /* Read Battery Monitor chip to get battery status data */
    status = ReadBatteryDataFromGasGauge(&batteryReadData);
  }
  else
  {
    ConsolePrintf("----->Gas Gauge status:%d v=%d\n\r",status, batteryReadData.BattVoltage);
  }

  /* confirm the lower battery state by read it once more */
  if (batteryReadData.BattVoltage <  BATTERY_LOW_THRESHOLD_MV)
  {
    int16_t preVoltage = (int16_t) read_register (BKP, RTC_BACKUP_BATTERY_VOLTAGE);  /* for both sleep states to confirm it is true */
    if (abs(preVoltage - batteryReadData.BattVoltage) >= BATTERY_SLEEP_VOLTAGE_DIFF)
    {
      /* gap over threshold, read it again to confirm */
      status = ReadBatteryDataFromGasGauge(&batteryReadData);
      if (abs(preVoltage - batteryReadData.BattVoltage) >= BATTERY_SLEEP_VOLTAGE_DIFF)
      {
        ConsolePrintf("----->Gas Gauge voltage confirmation s:%d p=%d, v=%d\n\r", status, preVoltage, batteryReadData.BattVoltage);
      }
      else
      {
        ; /* do nothing, the backup voltage will be updated before next sleep */
      }
    }
    else
    {
      ; /* do nothing, similar to previous voltage  */
    }
  }

  if (status == GASGAUGE_OK)
  {
    /* Calculate battery capacity */
    BatterymAh = valueRef + batteryReadData.BattChargeCount;

    /* Clip battery capacity to sensible range */
    if (BatterymAh < BATTERY_EMPTY_CAPACITY)
    {
      batteryCapacitymAh = BATTERY_EMPTY_CAPACITY;
    }
    else if (BatterymAh > BATTERY_FULL_CAPACITY)
    {
      batteryCapacitymAh = BATTERY_FULL_CAPACITY;
    }
    else
    {
      batteryCapacitymAh = BatterymAh;
    }
    batteryLevelPercent = GetBatteryLevelIndicator((int16_t) batteryCapacitymAh);
  }
  else
  {
    ConsolePrintf("ERROR bad read from Gas Gauge\n\r");
  }
  /* ConsolePrintf("valueRef=%d, BattChargeCount=%d, BatterymAh=%d, batteryCapacitymAh=%d\n\r", valueRef, batteryReadData.BattChargeCount, BatterymAh, batteryCapacitymAh); */
}


/**
 * @brief     Set the power state (normal / low / critical) used to control device operations
 *            POWER_STATE_NORMAL   - all operations allowed
 *            POWER_STATE_LOW      - no journey allowed
 *            POWER_STATE_CRITICAL - no journey or modem power up allowed
 * @param     (none)
 * @return    (none)
 */
static void BatteryMonitorCalculatePowerState (void)
{
  /* simple state machine to allow us to include hysteresis */
  if (UsbPresent)
  {
    PowerState = POWER_STATE_NORMAL;
  }
  else
  {
    switch (PowerState)
    {
      case POWER_STATE_NORMAL:
        if (batteryReadData.BattVoltage < BATTERY_LOW_THRESHOLD_MV)
        {
          PowerState = POWER_STATE_LOW;
          /* Resynchronize the battery charge level (we may have got out of sync over time) */
          BatteryMonitorEstimateCharge();
          /* add event to the log to indicate that the battery is low */
          StatusLogWriteEventCode(STATUS_BATTERY_LOW, BatteryChargeStatusPayloader());
          ConsolePrintf("PowerState = Low\n\r");
        }
        break;
      case POWER_STATE_LOW:
        /* check for change to lower power state */
        if (batteryReadData.BattVoltage < BATTERY_CRITICAL_THRESHOLD_MV)
        {
          PowerState = POWER_STATE_CRITICAL;
          /* add event to the log to indicate that the battery is depleted */
          StatusLogWriteEventCode(STATUS_BATTERY_EMPTY, BatteryChargeStatusPayloader());
          ConsolePrintf("PowerState = Critical\n\r");
        }
        /* check for change to higher power state */
        else if (batteryReadData.BattVoltage > BATTERY_LOW_THRESHOLD_MV + BATTERY_HYSTERESIS_MV)
        {
          PowerState = POWER_STATE_NORMAL;

          /* add event to the log to indicate that the battery is back to normal state its042 */
          StatusLogWriteEventCode(STATUS_BATTERY_NORMAL, BatteryChargeStatusPayloader());
          ConsolePrintf("PowerState = Normal\n\r");
        }
        break;
      case POWER_STATE_CRITICAL:
        /* check for change to higher power state */
        if (batteryReadData.BattVoltage > BATTERY_CRITICAL_THRESHOLD_MV + BATTERY_HYSTERESIS_MV)
        {
          PowerState = POWER_STATE_LOW;
          ConsolePrintf("PowerState = Low\n\r");
        }
        break;
    }
  }
}


/**
 * @brief     Manage the battery charging process
 * @param     None
 * @return    None
 */
static void BatteryMonitorHandleCharging (void)
{
  /* Check battery temperature to disable/enable charging if enabled */
  int16_t batteryTemp = (int16_t) (batteryReadData.BattTemperature / 10);

  if (((batteryTemp) < MINBATTCHARGETEMP) || ((batteryTemp) > MAXBATTCHARGETEMP))
  {
    if (ChargerEnabled)
    {
      /* its-069, allow 10 degrees over when it is already charge. Battery
       * could be 10 degrees cooler than the charge control chip where the sensor is read from */
      if (((batteryTemp) >= MINBATTCHARGETEMP) && (batteryTemp < (BATCHARGETEMPOVERRANGE + MAXBATTCHARGETEMP)))
      {
        ChargeTemperatureAllowed = true;  /* Charge over range on higher end only */
      }
      else
      {
        ChargeTemperatureAllowed = false;
      }
    }
    else
    {
      ChargeTemperatureAllowed = false;   /* disable charge */
    }
  }
  else
  {
    ChargeTemperatureAllowed = true;  /* Starting charge */
  }

  /* enable charging if requested and checks OK */
  if (ChargeTemperatureAllowed)
  {
    if (!ChargerEnabled)
    {
      ChargerEnableCharging();
      SetChargeConditionsChanged(true);
      ChargerEnabled = true;
    }
  }
  else
  {
    if (ChargerEnabled)
    {
      ChargerDisableCharging();
      SetChargeConditionsChanged(true);
      ChargerEnabled = false;
     }
  }

  /* Check if charging has changed state from charging to not charging */
  if (ChargeConditionsChanged && !UsbPresent)
  {
    BatteryChargeStateUpdate();
  }
}


/**
 * @brief     Check and update the charge status
 * @param     None
 * @return    None
 */
static void BatteryChargeStateUpdate (void)
{
  /* Increment the number of charge cycles.  In the future, we should store this to STC3100 RAM registers, which survive reboot & hibernation */
  Statistics.Battery.ChargeCycles++;
  /* Log the charge termination time and battery percent */
  Statistics.Battery.LastChargeTimestamp = RtcGetTimeStamp();
  Statistics.Battery.LastChargePercent = batteryLevelPercent;
  Statistics.Battery.LastChargeEndData = ChargeEndData;
  ConsolePrintf ("Charged %ds, Added %d%% to %d%% TS:%u\r\n", ((Statistics.Battery.LastChargeEndData>>16) & 0xffff),
      ((Statistics.Battery.LastChargeEndData>>8) & 0xff), Statistics.Battery.LastChargePercent, Statistics.Battery.LastChargeTimestamp);
}


/**
 * @brief     Check and update the USB state (present / not present)
 * @param     None
 * @return    None
 */
static void BatteryMonitorCheckUsbState (void)
{
  /* check for a change in USB Power state */
  if (PrevUsbState != UsbPresent)
  {
    PrevUsbState = UsbPresent;
    SetChargeConditionsChanged(true);
    ChargerSetInputCurrentLimit();

    /* its110, Run WiFi patch to keep WiFi chips in low power mode, as CE may be toggled 
        when USB plug and unplug 
    */
    WifiMemCEFix();

    /* log the status change for reporting to cloud */
    StatusLogWriteEventCode(UsbPresent ? STATUS_USB_ATTACH : STATUS_USB_DETACH, STATUS_DATA_LOAD_NONE);
  }
  /* update sleep mode. LP sleep reset mode apply when USB is not charging */
  UpdateLPSleepModeByChargeState();
  /* check shipping hibernate flag */
  if (!UsbPresent)
  {
    if (read_register (BKP, RTC_BACKUP_HIBERNATE_FLAG))
    {
      ConsolePrintf("\n\rHibernate Flag was Set ... \n\r\n\r");
      MotionMonitorHibernate();  /* this is a delayed hibernate action due to USB cable was powered */
    }
  }
}


/**
 * @brief     Set the charge condition change
 * @param     newState - the new state to set
 * @return    None
 */
static void SetChargeConditionsChanged (bool newState)
{
  ChargeConditionsChanged = newState;
  BatteryMeasurementHoldoff = 20;  /* four second hold-off to allow time for the gas gauge to update (STC3100 data sheet, section 7.2) */
}


/**
 * @brief     Print battery status information to the console
 * @param     None
 * @return    None
 */
static void BatteryMonitorPrintBatteryStatus (void)
{
  TRACE_BATMON("Battery Information (%d)\n\r", batteryReadData.BattCounter);
  TRACE_BATMON("> Coulomb:   %d\n\r", batteryReadData.CoulombCount);
  TRACE_BATMON("> Capacity:  %d mAh (%d%%)\n\r", batteryCapacitymAh, batteryLevelPercent);
  TRACE_BATMON("> Voltage:   %d mV\n\r", batteryReadData.BattVoltage);
  TRACE_BATMON("> Current:   %d mA\n\r", batteryReadData.BattCurrent);
  TRACE_BATMON("> Temp:      %d.%d C\n\r", batteryReadData.BattTemperature/10, batteryReadData.BattTemperature%10);
  sleep(5);
}


/**
 * @brief     Print charger status information to the console
 * @param     None
 * @return    None
 */
static void BatteryMonitorPrintChargerStatus (void)
{
  TRACE_BATMON("Charger Information\n\r");
  TRACE_BATMON("> USB Power: %s\n\r", UsbPresent ? "Present" : "Not present");
  switch (ChargingState)
  {
    case NOT_CHARGING:
      TRACE_BATMON("> Charging:  No\n\r");
      break;
    case CHARGING_IN_PROCESS:
    case CHARGING_IN_PROCESS_CAPACITY_FULL:
      TRACE_BATMON("> Charging:  Yes\n\r");
      break;
    case CHARGING_COMPLETE:
      TRACE_BATMON("> Charging:  Complete\n\r");
      break;
    default:
      break;
  }
  sleep(1);
}


/**
 * @brief     Print device power state
 * @param     None
 * @return    None
 */
static void BatteryMonitorPrintPowerState (void)
{
  TRACE_BATMON("Device Power State\n\r");
  switch (PowerState)
  {
    case POWER_STATE_NORMAL:
      TRACE_BATMON("> Normal\n\r");
      break;
    case POWER_STATE_LOW:
      TRACE_BATMON("> Low Power\n\r");
      break;
    case POWER_STATE_CRITICAL:
      TRACE_BATMON("> Critical\n\r");
      break;
  }
  sleep(1);
}

/**
 * @brief     Function to determine charging progress are finished
 * @          Current Implementation is by Gasgauge Current & Charge IC Register.
 * @param     none
 * @return    true if it is full, false otherwise
 */
static bool isChargingFull (void)
{
  bool chargerMarkFull = ChargerIsFullyCharged();
  bool gasgaugeMarkFull = (abs(batteryReadData.BattCurrent) < CHARGE_COMPLETE_CURRENT_MA) && (!BatteryMeasurementHoldoff);

  return (chargerMarkFull && gasgaugeMarkFull);

}

/**
 * @brief     Function to return the battery level indicator
 * @param     int16_t BattCapacity;     // battery capacity in mAh
 * @return    battery Level in percentage
 */
static uint32_t GetBatteryLevelIndicator (int16_t BattCapacity)
{
  // ensure there is no wrapping
  if (BattCapacity < BATTERY_EMPTY_CAPACITY)
  {
    BattCapacity = BATTERY_EMPTY_CAPACITY;
  }
  else if (BattCapacity > BATTERY_FULL_CAPACITY)
  {
    BattCapacity = BATTERY_FULL_CAPACITY;
  }

  uint32_t result = ((BattCapacity) * 100 / (BATTERY_FULL_CAPACITY));

  return (result);
}


/**
 * @brief     Return payload for the charging end status (Complete or not charging)
 * @param     currentTimestamp: timestamp when the charge end
 *            currentbatteryLevelPercent: battery level when the charge end
 * @return    charge end data for status Payload
 */
static uint32_t CalculateChargeEndData(uint32_t currentTimestamp, uint8_t currentBatteryLevel)
{
    uint32_t dt = currentTimestamp - ChargeStartTimeStamp;
    uint32_t chargeEndData = 0;
    if (dt > 0xffff)
    {
        dt = 0xffff;  /* protection */
    }
    TRACE_BATMON("Charge ended after %d s, to %d%%\r\n", dt, currentBatteryLevel);

    chargeEndData = ((0xffff & dt) << 16) | ((0xff & (currentBatteryLevel - ChargeStartPercent)) << 8) | (0xff & currentBatteryLevel);
    if ((currentBatteryLevel - ChargeStartPercent) >= CHARGE_PERCENT_MIN_ADDED)  /* at least 1% increase */
    {
        BatteryChargeStateUpdate();  /* update the Stats */
    }

    return chargeEndData;
}


/**
 * @brief     Return Current Battery Charging status, which can be one of three states:
 *              NOT_CHARGING                      : no USB power or charger disabled
 *              CHARGING_IN_PROCESS               : USB present, charger enabled, battery not full
 *              CHARGING_IN_PROCESS_CAPACITY_FULL : USB present, charger enabled, battery is BATTERY_FULL_CAPACITY
 *              CHARGING_COMPLETE                 : USB present, charger enabled, Gasguage and Charge IC reported Full
 *            Write the Charing related event code with the correponding payload
 * @param     none
 * @return    Charging state
 */
static ChargingState_e BatteryMonitorGetChargingStatus (void)
{
  static ChargingState_e PreviousChargingState =  UNKOWN_CHARGING_STATE;  /* init to unknown state */

  /* Update the charging state from USB status and batteryCapacitymAh */
  if (UsbPresent && ChargerEnabled)
  {
    /*
     * We should be charging since USB power is present and the charger is enabled.
     * To reliably tell if charging is complete 
     * 1. we read the charge current from gasguage is below the trickle charge threshold and 
     * 2. check with chargr IC is terminated the charging by the teminations current
     */
    if (isChargingFull())
    {
      ChargingState = CHARGING_COMPLETE;
    }
    else
    {
      ChargingState = CHARGING_IN_PROCESS;
      if (batteryCapacitymAh == BATTERY_FULL_CAPACITY)
      {
        ChargingState = CHARGING_IN_PROCESS_CAPACITY_FULL;
      }
    }
  }
  else
  {
    ChargingState = NOT_CHARGING;
  }

  /* ChargingState updated, */  
  TRACE_BATMON(" Charge state = [%d]>[%d] \r\n", PreviousChargingState, ChargingState);

  if (PreviousChargingState != ChargingState)
  {
    TRACE_BATMON("---> Charge PState=%u  CState=%d  Clevel=%d\r\n", PreviousChargingState, ChargingState, batteryLevelPercent);

    switch (ChargingState)
    {
      case CHARGING_IN_PROCESS:
        /* Start Charging, update the charging statistics */
        ChargeStartTimeStamp = RtcGetTimeStamp();
        ChargeStartPercent = batteryLevelPercent;
        TRACE_BATMON("Charge started %u-%d%%\r\n", ChargeStartTimeStamp, ChargeStartPercent);
        /* StatusLogWriteEventCode will be called later in the software when the current is established */
        ChargeStartLogDelayFlag = true;
        break;
      case CHARGING_COMPLETE:
      case NOT_CHARGING:
        /* Charging ended (Complete or not charging), calculate the Charge End Data for reporting */
        ChargeEndData = CalculateChargeEndData(RtcGetTimeStamp(), batteryLevelPercent);
        if (ChargingState == CHARGING_COMPLETE)
        {
          StatusLogWriteEventCode(STATUS_CHARGING_COMPLETE, ChargeEndData);
          /* Resynchronize the battery charge level */
          BatteryMonitorEstimateCharge();
        }
        else
        {
          /* Ignore UNKNOWN->TERMINATED state change from low power standby wakeup*/
          if (PreviousChargingState !=  UNKOWN_CHARGING_STATE)
          {
            StatusLogWriteEventCode(STATUS_CHARGING_TERMINATED, ChargeEndData);
          }
        }
        break;
      case CHARGING_IN_PROCESS_CAPACITY_FULL:
        /* Charging at full capacity */
        StatusLogWriteEventCode(STATUS_BATTERY_FULL, BatteryChargeStatusPayloader());
        break;
      default:
        break;
    }

    PreviousChargingState = ChargingState;
  }

  return ChargingState;
}


/*
 * @brief   Configure the USB Power detect pin and interrupt
 * @param   None
 * @retval  None
 */
static void BatteryMonitorConfigureGpio (void)
{
  set_gpio_input (USB_DETECT);
  set_gpio_enable_pulldown (USB_DETECT);
  
  /* USB_DETECT also needs to generate an interrupt on both edges */
  
  /* interrupt init */
  InterruptConfig.irqPrePriority = 6;
  InterruptConfig.irqSubPriority = 0;
  InterruptConfig.InterruptType = IO_INT_EDGE_BOTH;
  GpioConfigInterrupt(CHARGER_USB_DETECT_PORT, CHARGER_USB_DETECT_PIN, &InterruptConfig);

  /* Enable the Interrupt */
  GpioEnableInterrupt(CHARGER_USB_DETECT_PORT, CHARGER_USB_DETECT_PIN);
}


/*
 * @brief   Calculate estimated battery charge level based on known voltage discharge curve.
 *          This is run to create an initial estimate when the device is first powered on,
 *          and each time a charge cycle completes to keep things in sync.
 * @param   None
 * @retval  None
 */
static void BatteryMonitorEstimateCharge (void)
{
  /*
   * Reset the GasGauge accumulator since we are starting with a new charge estimate
   * and want to know charge in/out of the battery from this point.
   * Also prevents the accumulator from overflowing over time
   */
  GasGuageResetAccumulator();

  /* Read Battery Monitor chip to get battery status data */
  if (ReadBatteryDataFromGasGauge(&batteryReadData) == GASGAUGE_OK)
  {
    if (batteryReadData.BattVoltage >= 0)
    {
      double batteryVolts = batteryReadData.BattVoltage / 1000.0; // in volts
      uint16_t idx = 0;
      uint16_t batteryChargeLevel = 0;
      uint16_t valueRef = 0;
      // Estimate battery Charge level at first read using discharge curve equation at 250mA
      for (idx = 0; idx < BATTERYCURVESIZE; idx++)
      {
        // Assume battery full if condition met
        if (batteryVolts > batteryVoltageDischargeCurve[0])
        {
          batteryChargeLevel = BATTERY_FULL_CAPACITY;
          break;
        }
        else if (batteryVolts > batteryVoltageDischargeCurve[idx])
        {
          // Use index to extract corresponding charge level
          batteryChargeLevel = BATTERY_FULL_CAPACITY - batteryChargeDischargeCurve[idx];
          break;
        }
      }

      for (int m = 1; m <= 3; m++)   /* 3 repeat in case the RAM write failed, to target the 0% battery report bug */
      {
        /* Save first battery voltage value as reference in RAM location of Gas Gauge */
        if (GASGAUGE_WriteWord(GASGAUGE_REG_RAM0, batteryChargeLevel) == GASGAUGE_OK)
        {
          batteryCapacitymAh = batteryChargeLevel;
          batteryLevelPercent = GetBatteryLevelIndicator((int16_t) batteryCapacitymAh);
        }
        /* verify the GASGAUGE_REG_RAM0 writing */
        GASGAUGE_ReadWord(GASGAUGE_REG_RAM0, &valueRef);
        if (valueRef == batteryChargeLevel)
        {
          ConsolePrintf("Write to GASGAUGE_REG_RAM0 [@%d], mAh=%d, p=%d%% verified !\r\n", m, batteryCapacitymAh,batteryLevelPercent);
          break;
        }
        else
        {
          StatusLogWriteEventCode(STATUS_BATTERY_RAM0_ERROR, STATUS_DATA_LOAD_NONE);  /* log verify failed event */
        }
      }  /* for loop, will break on first good write operation */
    } /* batter voltage >= 0 */
    else
    {
      /* -ve voltage got from gas gauge */
      StatusLogWriteEventCode(STATUS_BATTERY_VOLTAGE_ERR, STATUS_DATA_LOAD_NONE);  /* No RAM0 update, previous value kept */
      ConsolePrintf("Error: Battery Gauge return Voltage= %d \r\n", batteryReadData.BattVoltage);
    }
  }
}


/*
 * @brief   Perform battery safety checks and hibernate the device if needed.
 *          Note: we use battery voltage rather than charge percentage, because
 *          voltage is an absolute measure, whereas charge percent is based on coulomb
 *          counting and can get out of sync over time.
 * @param   None
 * @retval  None
 */
static void BatteryMonitorSafetyChecks (void)
{
  if ((!UsbPresent) && (!ChargerIsChargingInProgress()))
  {
    /* Check for battery voltage (mV) < threshold */
    if (batteryReadData.BattVoltage < BATTERY_HIBERNATE_THRESHOLD_MV)
    {
      /* Set low battery shutdown flag, then hibernate the device */
      ConfigSetFlag(CONFIG_FLAG_LOWBAT_SHTDN, true);
      PowerStartHibernation();
    }
  }
}


/*
 * @brief  Update standby sleep mode from the USB status
 * @param   None
 * @retval  None.
 */
static void  UpdateLPSleepModeByChargeState  (void)
{
  if (UsbPresent)  /* uab charge cable */
  {
    SetStandbySleepMode (false);   /* deep sleep when it is Charge, over ride the config setting */
  }
  else
  {
    /* set from config setting  */
    if (STANDBY_SLEEP_SUPPORT == (STANDBY_SLEEP_SUPPORT & read_register (BKP, RTC_BACKUP_HARDWARE_TYPE)))  /* if the HW support */
    {
      SetStandbySleepMode (ConfigGetFlag(CONFIG_FLAG_STANDBY_LP_SLEEP));
    }
    else
    {
      SetStandbySleepMode (false);  /* over ride the config s sleep setting */
    }
  }
}


/*
 * @brief  build up and return the combined battery status bytes
 *          byte0: charge capacity in percentage 0-100
 *          byte1: gas gauge reported voltage, mv/100, 42= 4.2V bit7=1 indicate bq24292 charger chip.
 *          byte2: charge capacity in percentage mA/10, 112=1120mA
 *          byte3: gas gauge reported temperature in �C, bit7=1 indicate DCP port detected by FW.
 * @param   None
 * @retval  Uint32_T combined status log pay loader.
 */
static uint32_t BatteryChargeStatusPayloader (void)
{
  uint32_t returnBytes = 0;
  uint8_t flag = 0;
  BatteryMonitorCheckBatteryState();
  sleep(2);
  returnBytes = batteryLevelPercent;
  flag = (BATTERY_CHARGER_BQ24292 & ChargeStatusFlag())? 0x80 : 0x00;    /* BQ24292 flag */
  returnBytes |= ( (flag + ((abs(batteryReadData.BattVoltage)/100) & 0x7F)) << 8);
  returnBytes |= (((int)(abs(batteryReadData.BattCurrent)/10) & 0xFF) << 16);
  flag = (BATTERY_CHARGER_DCP_PORT & ChargeStatusFlag())? 0x80 : 0x00;
  returnBytes |= ( (flag + (abs(batteryReadData.BattTemperature/10) & 0x7F)) << 24);  /* DCP flag */
  ConsolePrintf("Charge state = 0x%X flag=[%X]\n\r", returnBytes, ChargeStatusFlag());
  return (returnBytes);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Display current battery status
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int BatteryMonitorConsoleStatus (int argCount, char **pArguments)
{
  /* update the status so we have current information */
  BatteryMonitorGetChargingStatus();
  BatteryMonitorCheckBatteryState();

  /* print the status details to console */
  BatteryMonitorPrintBatteryStatus();
  BatteryMonitorPrintChargerStatus();
  BatteryMonitorPrintPowerState();
  BatteryChargeStatusPayloader();

  TRACE_BATMON("> Threshold: %d %d %d\n\r",BATTERY_LOW_THRESHOLD_MV, BATTERY_CRITICAL_THRESHOLD_MV, BATTERY_HIBERNATE_THRESHOLD_MV );
  sleep(1);
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Turn periodic battery logging on or off
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int BatteryMonitorConsoleLog (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "on"))
    {
      BatteryLogging = true;
      ConsolePrintf("Battery logging ON\n\r");
    }
    if (!strcmp(pArguments[0], "off"))
    {
      BatteryLogging = false;
      ConsolePrintf("Battery logging OFF\n\r");
    }
    return(CONSOLE_ERROR_OK);
  }

  return(CONSOLE_ARG_NO);
}

/**
 * @brief       Force the battery status sync with the real voltage aganist the discharging curve
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int BatteryMonitorConsoleSync (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    BatteryMonitorEstimateCharge();
    BatteryMonitorCheckBatteryState();
    /* print the status details to console */
    BatteryMonitorPrintBatteryStatus();
    return(CONSOLE_ERROR_OK);
  }

  return(CONSOLE_ARG_NO);
}



