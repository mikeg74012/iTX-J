/**
 ******************************************************************************
 * @file   hardwareversion.c
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   07/08/2023
 *
 * @brief  hardware detection Driver
 *
 ******************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2023
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define USER_BUT_DETECT_DELAY             (200)     /* Delay value for the wakup pulse detection with user Button */

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void HWSettingForHWVersion (int hwVersion);

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */

/**
 * @brief     Checking the hardware versions and set the system flag
 * @param     none
 * @return    none
 */
void HardwareVersionCheck (void)
{
  if (isPCBVersionF())
  {
    SetHardwareVersion(HW_PCB_VERSION_F);
  }
  else if (isPCBVersionDE())
  {
    #ifdef GD32F30X_HD
      /*no HW D2 detection for GD MCU */
      SetHardwareVersion(HW_PCB_VERSION_E);
    #else
      if (isPCBVersionD2())
      {
        SetHardwareVersion(HW_PCB_VERSION_D);
      }
      else
      {
        if (ChargerIsBQ24292())
        {
          SetHardwareVersion(HW_PCB_VERSION_E);
        }
        else
        {
          SetHardwareVersion(HW_PCB_VERSION_D);
        }
      }
    #endif
  }
  else if (isPCBVersionA())
  {
    SetHardwareVersion(HW_PCB_VERSION_A);
  }
  else
  {
    SetHardwareVersion(HW_PCB_VERSION_BC);  /* default */
  }
}


/**
 * @brief     Checking the hardware feature to determine the hardware versions
 *            PCB-F: PB1 pulled own via 2K R, unique feature
 * @param     none
 * @return    true if correct version detected
 */
bool isPCBVersionF (void)
{
  bool rtnFlag = true;
  gpioconfig_t PinConfig;

  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_UP;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DF_PORT, HWBOARD_VERSION_DF_PIN, &PinConfig);

  /* Check if HW able to keep GNSS on when GSM off */
  if (GpioReadPin(HWBOARD_VERSION_DF_PORT, HWBOARD_VERSION_DF_PIN) == IO_PIN_RESET)
  {
    rtnFlag = true;
  }
  else
  {
    rtnFlag = false;
  }
  /* Reset the pull-up of the IO */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DF_PORT, HWBOARD_VERSION_DF_PIN, &PinConfig);
  return (rtnFlag);
}


/**
 * @brief     Checking the hardware feature to determine the hardware versions
 *            PCB-D/E: PA0 pulled up, detect low when one of the logic chip input set  to low
 * @param     none
 * @return    true if correct version detected
 */
bool isPCBVersionDE (void)
{
  bool rtnFlag = true;
  gpioconfig_t PinConfig;
  int i = 0;
  /* input pin */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_UP;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DDE_PORT, HWBOARD_VERSION_DDE_PIN, &PinConfig);
  /* output pin*/
  PinConfig.Direction = IO_OUTPUT_PP;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_USER_BUT_PORT, HWBOARD_VERSION_USER_BUT_PIN, &PinConfig);
  /* set to low */
  GpioWritePin(HWBOARD_VERSION_USER_BUT_PORT, HWBOARD_VERSION_USER_BUT_PIN, IO_PIN_SET);
  GpioWritePin(HWBOARD_VERSION_USER_BUT_PORT, HWBOARD_VERSION_USER_BUT_PIN, IO_PIN_RESET);
  /* wait 100 us*/
  do
  {
    GpioReadPin(HWBOARD_VERSION_DDE_PORT, HWBOARD_VERSION_DDE_PIN);
    i++;
  } while (i < USER_BUT_DETECT_DELAY);
  /* check the pin */
  if (GpioReadPin(HWBOARD_VERSION_DDE_PORT, HWBOARD_VERSION_DDE_PIN) == IO_PIN_RESET)
  {
    rtnFlag = true;
  }
  else
  {
    rtnFlag = false;
  }
  /* Reset the IOs */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DDE_PORT, HWBOARD_VERSION_DDE_PIN, &PinConfig);
  GpioInitPin(HWBOARD_VERSION_USER_BUT_PORT, HWBOARD_VERSION_USER_BUT_PIN, &PinConfig);
  return (rtnFlag);
}


/**
 * @brief     Checking the hardware feature to determine the hardware versions
 *            PCB-D/E: Version D2 this pin is high after init HW check check
 * @param     none
 * @return    true if correct version detected
 */
bool isPCBVersionD2 (void)
{
  bool rtnFlag = true;
  gpioconfig_t PinConfig;
  /* input pin pull down first */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_DOWN;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DD2_PORT, HWBOARD_VERSION_DD2_PIN, &PinConfig);
  /* input pin pull up for detection */
  PinConfig.Pull = IO_PULL_UP;
  GpioInitPin(HWBOARD_VERSION_DD2_PORT, HWBOARD_VERSION_DD2_PIN, &PinConfig);
  /* HW D2, no 47k pull down on this pin, it will show HIGH state */
  if (GpioReadPin(HWBOARD_VERSION_DD2_PORT, HWBOARD_VERSION_DD2_PIN) == IO_PIN_SET)
  {
    rtnFlag = true;
  }
  else
  {
    rtnFlag = false;
  }
  /* Reset the IOs */
  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(HWBOARD_VERSION_DD2_PORT, HWBOARD_VERSION_DD2_PIN, &PinConfig);
  return (rtnFlag);
}


/**
 * @brief     Checking the hardware feature to determine the hardware versions
 *            PCB-A: PA7 pulled on the modem
 * @param     none
 * @return    true if correct version detected
 */
bool isPCBVersionA (void)
{
  bool rtnFlag = true;
  gpioconfig_t PinConfig;

  PinConfig.Direction = IO_INPUT_DIGITAL;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN, &PinConfig);

  /*
   * Detect whether the Modem enable pin uses negative logic (HW issue A) or
   * positive logic (HW issue B+).
   * We detect by reading the un-driven state of the pin to see whether it is
   * pulled up or down.
   */
  if (GpioReadPin(GSM_POWER_ENABLE_PORT, GSM_POWER_ENABLE_PIN) == IO_PIN_SET)
  {
    rtnFlag = true;
  }
  else
  {
    rtnFlag = false;
  }

  return (rtnFlag);
}


/**
 * @brief     Write hardware version number to RTC register
 *            and apply the hw setting with the hw version
 * @param     hwVersion: detected hardware version number
 * @return    none
 */
void SetHardwareVersion (uint8_t hwVersion)
{
  /* Write the hw version to the RTC */
  RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_VERSION, hwVersion);  
  /* Set the HW related setting for the hw version from RTC */
  HWSettingForHWVersion(GetHardwareVersion());
  return;
}

/**
 * @brief     Read hardware version number from RTC register
 * @param     none
 * @return    hardware version read back from register
 */
int GetHardwareVersion (void)
{
  int hwVersion = RtcBkupReadRegister(RTC_BACKUP_HARDWARE_VERSION);
  return hwVersion;
}

/**
 * @brief     Apply the HW setting according to the hardware version
 * @param     hwVersion of the Board
 * @return    none
 */
static void HWSettingForHWVersion (int hwVersion)
{  
  switch (hwVersion)
  {
    case HW_PCB_VERSION_F:
      ModemSupportGSMOffMode(true);
      RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, STANDBY_SLEEP_SUPPORT);
      #ifdef DEBUG
        ConsolePrintf("HW version F, support Modem power stand alone and standby sleep mode \r\n");
      #endif
      break;
    case HW_PCB_VERSION_E:
      ModemSupportGSMOffMode(false);
      RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, STANDBY_SLEEP_SUPPORT);
      #ifdef DEBUG
        ConsolePrintf("HW version E, support standby sleep mode \r\n");
      #endif
      break;
    case HW_PCB_VERSION_D:
      ModemSupportGSMOffMode(false);
      RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, DEEP_SLEEP_SUPPORT);
      #ifdef DEBUG
        ConsolePrintf("HW version D detected \r\n");
      #endif
      break;
    case HW_PCB_VERSION_A:
      ModemSupportGSMOffMode(false);
      RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, DEEP_SLEEP_SUPPORT);
      #ifdef DEBUG
        ConsolePrintf("HW version A detected \r\n");
      #endif
      break;
    default:
      ModemSupportGSMOffMode(false);
      RtcBkupWriteRegister(RTC_BACKUP_HARDWARE_TYPE, DEEP_SLEEP_SUPPORT);
      #ifdef DEBUG
        ConsolePrintf("HW version B/C detected \r\n");
      #endif
      break;
  }
  return;
}


