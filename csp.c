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
    sleep(1);
    
    /* set sleepdeep bit of Cortex-M4 system control register */
    or_register (SCB, SCB_SCR, (1 << 2));

    /* set stbmod bit */
    or_register (PMU, PMU_CTL, (1 << 1));
        
    /* reset wakeup flag */
    or_register (PMU, PMU_CTL, (1 << 2));
    
    /* select WFI or WFE command to enter standby mode */
    
    __WFI();
  }
  else
  {
        static uint32_t reg_snap[ 4 ];     
      
        /* clear stbmod and ldolp bits */
        and_register (PMU, PMU_CTL, ~((uint32_t)((1 << 1) | (1 << 0) | (3 << 18) | (1 << 11) | (1 << 10))));

        /* set ldolp bit according to pmu_ldo */
        or_register (PMU, PMU_CTL, (1 << 0));

        /* low drive mode config in deep-sleep mode */
        or_register (PMU, PMU_CTL, (uint32_t) ((3 << 18) | (1 << 10)));

        /* set sleepdeep bit of Cortex-M4 system control register */
        or_register (SCB, SCB_SCR, (1 << 2));

        reg_snap[ 0 ] = read_register (0xE000E010U, 0);
        reg_snap[ 1 ] = read_register (0xE000E100U, 0);
        reg_snap[ 2 ] = read_register (0xE000E104U, 0);
        reg_snap[ 3 ] = read_register (0xE000E108U, 0 );

        and_register (0xE000E010U, 0, 0x00010004U);
        write_register (0xE000E180U, 0, 0xFF7FF83DU);
        write_register (0xE000E184U, 0, 0XFFFFF8FFU);
        write_register (0xE000E188U, 0, 0xFFFFFFFFU);     

        /* select WFI or WFE command to enter deepsleep mode */

        __SEV();
        __WFE();
        __WFE();

        write_register (0xE000E010U, 0, reg_snap[ 0 ]) ; 
        write_register (0xE000E100U, 0, reg_snap[ 1 ]) ;
        write_register (0xE000E104U, 0, reg_snap[ 2 ]) ;
        write_register (0xE000E108U, 0, reg_snap[ 3 ]) ;  

        /* reset sleepdeep bit of Cortex-M4 system control register */
        and_register (SCB, SCB_SCR, ~(1 << 2));
        
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

