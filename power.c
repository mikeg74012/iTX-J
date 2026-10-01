/**
 **************************************************************************************************
 * @file   power.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   10/12/2019
 *
 * @brief  Power management functions
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
#if TRACE_POWER_ENABLE
#define TRACE_POWER(...) TRACE_("POWER", __VA_ARGS__)
#else
  #define TRACE_POWER(...)
#endif


#define SET_HIBERNATE_FLAG              0x01
#define RESET_HIBERNATE_FLAG            0x0

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
eResetCause_t ResetCause = RESET_UNKNOWN;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Shutdown peripherals
 * @param     None
 * @return    None
 */
void PowerPutPeripheralsToSleep (void)
{
  /* Put LED module to sleep */
  LedSleep();
  /* Deactivate PSRAM Chip Select */
  
  set_gpio_high (SPI_CS_SRAM);
}


/**
 * @brief     Wakeup peripherals
 * @param     None
 * @return    None
 */
void PowerWakeupPeripherals (void)
{
  /* TBD - power-up anything which is not handled elsewhere */
  //ledWake();
}


/**
 * @brief Start Hibernation (MCU Standby)
 * @warning This function only returns if USB power is present
 *          If / when USB power is removed, the system is completely powered down,
 *          and the MCU will restart from a cold boot when USB power is resumed
 * @param   None
 * @retval  None
 */
void PowerStartHibernation (void)
{
  if (BatteryMonitorGetUsbState())
  {
    /*
     * USB powered, set flag and return, It will set to ship mode when the USB is powered off
     * No battery disconnect when the USB is powered.
     * This allows the battery charged after received hibernate request.
     */
      write_register (BKP, RTC_BACKUP_HIBERNATE_FLAG, SET_HIBERNATE_FLAG);      /* set the flag */
    ConsolePrintf("Will set Battery to shipping mode when USB power is off! \n\r\n\r");
    return;
  }

  ConfigWriteWustFlag(WUST_NOT_DONE);

  ConsolePrintf("\r\n!!! Hibernating MCU now !!!\n\r");
  write_register (BKP, RTC_BACKUP_REGISTER_RESET_CAUSE, RESET_HIBERNATE);
  write_register (BKP, RTC_BACKUP_HIBERNATE_FLAG, RESET_HIBERNATE_FLAG);      /* reset flag before hibernate */

  PowerPutPeripheralsToSleep();
  GASGAUGE_Powerdown();
  ChargerEnterShippingMode();

  /**********************************************************/
  /*                    MCU POWER DOWN                      */
  /*      WARNING - ONLY RETURNS IF USB POWER PRESENT       */
  /**********************************************************/
}


/**
 * @brief   Reset the MCU
 * @warning This function reboots the processor
 * @warning This function does not return
 * @param   None
 * @retval  None
 */
void PowerResetMCU (void)
{
  /*
   * Reboot the MCU
   * WARNING - Never returns
   */
  ConsolePrintf("!!! Resetting MCU now !!!\n\r");
  MessageLPSSetPacketCount(0);  /* reset packet count */
  SetBackupResetRegister(RESET_SOFTWARE);
  sleep(1000);
  SystemReset();


  /**********************************************************/
  /*                       MCU RESET                        */
  /*          WARNING - FUNCTION DOES NOT RETURN            */
  /**********************************************************/
}


/**
 * @brief     Set the reset backUP register bit, Clear the LP Sleep mark if need post report
 * @param     none
 * @return    eResetCause_t
 */
void SetBackupResetRegister (uint32_t resetCause)
{
  /* write the reset cause to RTC backup reg */
  write_register (BKP, RTC_BACKUP_REGISTER_RESET_CAUSE, resetCause);
  ConsolePrintf("Reset Cause = 0x%x \r\n", resetCause);
  /* write the flag */
  UpdateResetDataToRAM(PSRAM_SYSTEM_FAULT_RESET_MARK);  /* save current data to RAM before reset */
}


/**
 * @brief     Returns the Reset cause of device (clears flag after call)
 * @param     none
 * @return    eResetCause_t
 */
eResetCause_t PowerGetResetCause (void)
{
  static bool FirstCall = true;
    
  /* On the first call, we read the hardware.  Subsequent calls just return the stored value */
  if (FirstCall)
  {
    FirstCall = false;

    /* Check if device was hibernated because of a low battery */
    if (ConfigGetFlag(CONFIG_FLAG_LOWBAT_SHTDN))
    {
      /* This flag in the flash need to be reset after Console inited */
      ResetCause = RESET_LOW_BATTERY;
    }
    else
    {
#ifdef GD32F30X_HD  // get the MCU Rest cause for GD32
        
        /* Check LVDF flag */
        
        if (read_register (PMU, PMU_CS) & (1 << 2))
      {
        ResetCause = RESET_LOWPOWER;
      }
      else
      {
        ResetCause = RESET_UNKNOWN;
      }
#else
      /* check the MCU reset cause register */
      if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
      {
        ResetCause = RESET_LOWPOWER;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
      {
        ResetCause = RESET_EXTERNALRESETPIN;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
      {
        ResetCause = RESET_SOFTWARE;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET)
      {
        ResetCause = RESET_EXTERNALRESETOPTBYT;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
      {
        ResetCause = RESET_EXTERNALRESETPORPDR;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
      {
        ResetCause = RESET_WATCHDOGWIN;
      }
      else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
      {
        ResetCause = RESET_WATCHDOGIDG;
      }
      else
      {
        ResetCause = RESET_UNKNOWN;
      }
      /* Clear reset flags in any case */
      __HAL_RCC_CLEAR_RESET_FLAGS();
#endif
    }
  }
  return ResetCause;
}


/**
 * @brief     Override the Unknown ResetCause if there is info in the RTC Backup Register
 * @param     none
 * @return    eResetCause_t
 */
eResetCause_t PowerGetResetCauseFromRTCBkupRegister (void)
{
  if (ResetCause == RESET_UNKNOWN)
  {
    /* get the MCU Rest cause for from the RTC Backup Register */
    ResetCause = (eResetCause_t) read_register (BKP, RTC_BACKUP_REGISTER_RESET_CAUSE);
  }
  /* Reset the Bkup register cause in the backup register after read */
  write_register (BKP, RTC_BACKUP_REGISTER_RESET_CAUSE, RESET_BKUP_NONE);

  return ResetCause;
}







