/**
 **************************************************************************************************
 * @file   post.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   27/01/2022
 *
 * @brief  power on self test module
 *
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the Power System */
#if TRACE_POST_ENABLE
#define TRACE_POST(...) TRACE_("POST", __VA_ARGS__)
#else
  #define TRACE_POST(...)
#endif

#define OUTPUTWUSTREPORTSNAP          /* use to print WUST report info on console   */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static WUST_ReportInfo_t deviceWUSTReport = { 0 };


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void PostPrintWUSTReport (void);
static void PostPrintResetCause (void);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Carry out Wake-Up Self-Test on device if required
 * @param     eWUSTDeviceID_t deviceTypeID
 * @return    None
 */
void PostWakeUpSelfTest (eWUSTDeviceID_t deviceTypeID)
{
  if (IsLowPowerSleepFlagUp())
  {
    int maj,min,pat;
    /* update static version strings as LP sleep reset them */
    if (sscanf((char*)FIRMWARE_VERSION_STRING, "%d.%d.%d", &maj, &min, &pat) == 3)
    {
      deviceWUSTReport.AppFirmwareVersion.MajorVersion = maj;
      deviceWUSTReport.AppFirmwareVersion.MinorVersion = min;
      deviceWUSTReport.AppFirmwareVersion.PatchVersion = pat;
    }
    else
    {
      TRACE_POST("***Error: WUST: Failed to get Application firmware version***\n\r");
    }
    return;  /* No POST for LP sleep Reset */
  }

  switch (deviceTypeID)
  {
    case IMU:
      if (AccelDriverCheckWhoAmI())
      {
        // Update IMU WUST Pass Flag
        deviceWUSTReport.WUSTFlag |= (1 << ((IMU * 2) + 1));
      }
      // Update IMU WUST Done Flag
      deviceWUSTReport.WUSTFlag |= (1 << IMU * 2);
      break;
    case SRAM:
      if (SerialRamWUST())
      {
        // Update SRAM WUST Pass Flag
        deviceWUSTReport.WUSTFlag |= (1 << ((SRAM * 2) + 1));
      }
      // Update SRAM WUST Done Flag
      deviceWUSTReport.WUSTFlag |= (1 << SRAM * 2);
      break;
    case MODEM:
      if (ModemWUST())
      {
        deviceWUSTReport.WUSTFlag |= (1 << ((MODEM * 2) + 1));
      }
      deviceWUSTReport.WUSTFlag |= (1 << MODEM * 2);
      break;
    case WIFI:
      // Update WIFI WUST Done Flag
      deviceWUSTReport.WUSTFlag |= (1 << WIFI * 2);
      // TODO:Update WIFI WUST Pass Flag
      // deviceWUSTReport.WUSTFlag |= (1 << ((WIFI * 2) + 1));
      break;
    case BATTERYCHARGER:
      if (ChargerWUST())
      {
        // Update BATTERY CHARGER WUST Pass Flag
        deviceWUSTReport.WUSTFlag |= (1 << ((BATTERYCHARGER * 2) + 1));
      }
      // Update BATTERY CHARGER WUST Done Flag
      deviceWUSTReport.WUSTFlag |= (1 << BATTERYCHARGER * 2);
      break;
    case GASGUAGE:
      if (GASGAUGE_WUST())
      {
        // Update GAS GUAGE WUST Pass Flag
        deviceWUSTReport.WUSTFlag |= (1 << ((GASGUAGE * 2) + 1));
      }
      // Update GAS GUAGE WUST Done Flag
      deviceWUSTReport.WUSTFlag |= (1 << GASGUAGE * 2);
      break;
    default:
      break;
  }

  /* Only write back to flash when all checks are done */
  if ((deviceWUSTReport.WUSTFlag & WUST_DONE) == WUST_DONE)
  {
    /* Prepare deviceWUSTReport */
    AccelerationData_t accelData;
    int maj,min,pat;

    deviceWUSTReport.resetCause = PowerGetResetCause();
    deviceWUSTReport.batteryPercentage = BatteryMonitorGetLevel();
    XyzGetFilteredData(&accelData);
    deviceWUSTReport.accelDataRef = accelData;

    if (sscanf((char*)FIRMWARE_VERSION_STRING, "%d.%d.%d", &maj, &min, &pat) == 3)
    {
      deviceWUSTReport.AppFirmwareVersion.MajorVersion = maj;
      deviceWUSTReport.AppFirmwareVersion.MinorVersion = min;
      deviceWUSTReport.AppFirmwareVersion.PatchVersion = pat;
    }
    else
    {
      TRACE_POST("***Error: WUST: Failed to get Application firmware version***\n\r");
    }

    ConfigWriteWustFlag(deviceWUSTReport.WUSTFlag);
    PostPrintWUSTReport();

    /* Send self test result to the cloud */
    if (false == TestModeGet())  /* wake up from Hibernate or reset */
    {
      SequencerSelfTest();
      MessageLPSSetPacketCount(0) ;  /* reset packet count */
    }
    else
    {
      TRACE_POST("Test Mode - SKipping SelfTest\n\r");
    }
  }
}


/**
 * @brief     Sets the Battery Info reading required in WUST report info
 * @param     GASGAUGE_BatteryStatus_t batteryInfo
 * @return    None
 */
void PostWUSTSetBatteryInfo (GASGAUGE_BatteryStatus_t batteryInfo)
{
  deviceWUSTReport.batteryData = batteryInfo;
}


/**
 * @brief     Returns the Wake Up Self test report information snapshot
 * @param     None
 * @return    WUST_ReportInfo_t wustReport to return information
 */
WUST_ReportInfo_t getWUSTReportInfoSnapShot (void)
{
  return deviceWUSTReport;
}


static void PostPrintWUSTReport (void)
{
#ifdef OUTPUTWUSTREPORTSNAP
  TRACE_POST("====== WUST Information =======\n\r");
  PostPrintResetCause();
  TRACE_POST(" > Flag: 0x%04X\n\r", deviceWUSTReport.WUSTFlag);
  TRACE_POST(" > Battery state: %d %%\n\r", deviceWUSTReport.batteryPercentage);
  TRACE_POST(" > Battery voltage: %d  mV\n\r", deviceWUSTReport.batteryData.BattVoltage);
  TRACE_POST(" > Battery current: %d mA\n\r", deviceWUSTReport.batteryData.BattCurrent);
  TRACE_POST(" > Battery temperature: %d.%d C\n\r", deviceWUSTReport.batteryData.BattTemperature / 10,
      deviceWUSTReport.batteryData.BattTemperature % 10);
  /*
   * Convert to signed value, then scale to range to units
   * of G/1000 in range -16g to +16g. Use U32 for
   * intermediate value to avoid overflow
   */
#if TRACE_POST_ENABLE
  uint32_t intermediateValue = ((int32_t) deviceWUSTReport.accelDataRef.X) * 1000;
  int16_t accelX = (int16_t) (intermediateValue / 2048);
  intermediateValue = ((int32_t) deviceWUSTReport.accelDataRef.Y) * 1000;
  int16_t accelY = (int16_t) (intermediateValue / 2048);
  intermediateValue = ((int32_t) deviceWUSTReport.accelDataRef.Z) * 1000;

  int16_t accelZ = (int16_t) (intermediateValue / 2048);
  TRACE_POST(" > IMU Ref: X=%dmg, Y=%dmg, Z=%dmg\n\r", accelX, accelY, accelZ);
#endif
#endif
}


/**
 * @brief     Prints the Power On WUST reset cause to console (for debug purposes)
 * @param     eWUSTResetSource_t aResetCause
 * @return    None
 */
static void PostPrintResetCause (void)
{

#ifdef OUTPUTWUSTREPORTSNAP
  switch (deviceWUSTReport.resetCause)
  {
  case RESET_LOW_BATTERY:
    TRACE_POST(" > Reset Cause: LOW BATTERY HIBERNATE\n\r");
    break;
  case RESET_WATCHDOGWIN:
    TRACE_POST(" > Reset Cause: WATCHDOG (WINDOW)\n\r");
    break;
  case RESET_LOWPOWER:
    TRACE_POST(" > Reset Cause: LOWPOWER\n\r");
    break;
  case RESET_EXTERNALRESETPIN:
    TRACE_POST(" > Reset Cause: EXTERNALRESET (PIN)\n\r");
    break;
  case RESET_SOFTWARE:
    TRACE_POST(" > Reset Cause: SOFTWARE\n\r");
    break;
  case RESET_EXTERNALRESETOPTBYT:
    TRACE_POST(" > Reset Cause: EXTERNALRESET (OPTION BYTEs)\n\r");
    break;
  case RESET_EXTERNALRESETPORPDR:
    TRACE_POST(" > Reset Cause: EXTERNALRESET (POR/PDR)\n\r");
    break;
  case RESET_WATCHDOGIDG:
    TRACE_POST(" > Reset Cause: WATCHDOG (ING)\n\r");
    break;
  case RESET_HIBERNATE:
    TRACE_POST(" > Reset Cause: HIBERNATE\n\r");
    break;
  case RESET_BATTERYREMOVED:
    TRACE_POST(" > Reset Cause: BATTERY REMOVED\n\r");
    break;
  case RESET_RB_LHOLD:
    TRACE_POST(" > Reset Cause: RED BUTTON RESET\n\r");
    break;
  case RESET_FW_UPDATE:
    TRACE_POST(" > Reset Cause: OTA FIRMWARE UPDATE\n\r");
    OTAUpdateFWVerify();
    break;
  case RESET_BL_UPDATE:
    TRACE_POST(" > Reset Cause: OTA BOOTLOADER UPDATE\n\r");
    break;
  case RESET_SYSFAULT_NMI_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT NMI\n\r");
    break;
  case RESET_SYSFAULT_HARD_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT HARD\n\r");
    break;
  case RESET_SYSFAULT_MEMMANAGE_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT MEMMANAGE\n\r");
    break;
  case RESET_SYSFAULT_BUS_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT BUS\n\r");
    break;
  case RESET_SYSFAULT_USAGE_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT USAGE\n\r");
    break;
  case RESET_SYSFAULT_DEBUGMON_FAULT:
    TRACE_POST(" > Reset Cause: SYSFAULT DEBUGMON\n\r");
    break;
  case RESET_BKUP_NONE:
  case RESET_SYSFAULT_NONE:
  case RESET_UNKNOWN:
  default:
    TRACE_POST(" > Reset Cause: UNKNOWN\n\r");
    break;
  }

#endif
}
