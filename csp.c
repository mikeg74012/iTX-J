/**
 **************************************************************************************************
 * @file   csp.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Top Level Chip Support for GD32
 *         include power up setup and sleep mode functions
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PUBLIC VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       set mcu to sleep
 * @param[in]   none
 * @return      none
 */
void SystemSleep (void)
{
  if (IsLowPowerSleepFlagUp())
  {
    /* wake up @ MCU reset */
    SetSleepCount(GetSleepCount() + 1);
    or_register (PMU, PMU_CTL, (1 << 3) | (1 << 2));        /* Clear wakeup and standby flags */

    or_register (PMU, PMU_CS, (1 << 8));                 /* Enable Wakeup pin */
   
/* clear flag and enable WKUP pin before standby sleep */
   /* ConsolePrintf("--> Sleep: PMU_STATUS_REG = 0X%0X Flags=0x%x\r\n\r\n", PMU_CS, GetLPSleepStatus()); */
    ConsolePrintf("\r\n    zzz ... \r\n", PMU_CS, GetLPSleepStatus());
    sleep(1);
    pmu_to_standbymode(WFI_CMD); /* standby cmd */
  }
  else
  {
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFE_CMD); /* deep sleep cmd */
  }
}


/**
 * @brief  Enable the Free WatchDog. Once enabled, it cannot disable until MCU reset
 * @param  none
 * @retval int (not used)
 */
void SystemFreeWatchDogEnable(void)
{
#if 0
/* Don't bother with the watchdog for the moment */
    
  /* Enable the watchdog config */
  fwdgt_write_enable();

  /* Get the longest possible value for the watch dog i.e. 26214.4ms */
  fwdgt_config(0x0FFF, FWDGT_PSC_DIV256);   //40khz/256= 156hz
  /* No more config on the watchdog */
  fwdgt_write_disable();

  /* Reload the watchdog and fly off */
  fwdgt_counter_reload();
  fwdgt_enable();

#endif
  
}

/**
 * @brief  Reload the Free WatchDog
 * @param  none
 * @retval int (not used)
 */
void SystemFreeWatchDogReload(void)
{
#if 0
/* Don't bother with the watchdog for the moment */
  fwdgt_counter_reload();
#endif
  
}

/*
 **************************************************************************************************
 *                                         PRIVATE SECTION
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

